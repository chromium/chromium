# How does loading WebView work?

[TOC]

## Summary

This is a technical explanation of how loading WebView into an Android app
actually works, including details of how this varies across different versions
of Android and the bugs and limitations that exist. This is important if making
changes to how WebView is packaged, as we typically need to maintain backward
compatibility. This may not be 100% complete as a lot of things have changed
over the years that WebView has been updatable. This currently covers Android L
to Q.

## General stuff

The WebView implementation was moved out of the framework and into a separate
APK in L. All the core framework code is automatically present in every app at
runtime, but code in additional APKs and libraries is not, so this change
required that we create a mechanism to load it automatically when the app uses
WebView.

*** aside
In N, we made it possible for the Chrome APK to also provide a WebView
implementation to save space, and introduced mechanisms to enable switching
between different implementations. This is discussed in more detail in the
[guide to WebView packaging variants](webview-packaging-variants.md) and in the
[channel switching documentation](channels.md). For the purposes of this doc,
you can assume all references to "the APK" or "the WebView APK" refer to the
currently selected WebView implementation - loading is handled the same way.
Switching WebView provider is also handled the same way as updating the current
provider.
***

The APK contains three categories of "stuff": compiled Java code (one or more
`.dex` files), compiled native code (one or more `.so` files), and non-code data
such as assets and resources. Each of these is discussed in its own section, but
they are not completely independent of each other and are best read in order.

The loading process begins when an application first instantiates one of the
classes or calls one of the static methods in the framework's `android.webkit`
package - most classes in that package require the WebView implementation to be
loaded to be used. This includes inflating an XML layout that contains a
WebView, since this constructs a WebView instance.

The process is driven by the `WebViewFactory` class in the framework, and most
of the code involved in loading the implementation is also part of the
framework. This means that it can't be updated outside of the Android platform
release cycle, somewhat limiting our flexibility as we have to remain backward
compatible with old versions of the loading code.

## Getting a Context

The Android platform provides a supported mechanism for applications to load and
access the APKs of other installed apps: the
[`Context.createPackageContext`](https://developer.android.com/reference/android/content/Context#createPackageContext%28java.lang.String,%20int%29)
method, which returns a `Context` object for that app's APK. The WebView loading
mechanism uses this on L and M.

A side effect of `createPackageContext` is that `ActivityManager` is notified
that the calling process has loaded the APK in question. This is used to decide
which processes should be killed if that APK is uninstalled or updated, and thus
when WebView is updated, all app processes that have loaded it are killed, and
will use the new version if they are restarted.

While this is often frustrating for app developers, it would cause problems if
we didn't kill these processes:

* The process might be in the middle of loading WebView at the time. The APK is
accessed multiple times during loading and if it's been replaced in the meantime
then we might fail to load some part, or load mismatched versions.

* The process is likely to already have open file handles referring to the APK.
This will prevent the APK from actually being deleted until it exits, which may
not be for a long time in the case of some apps (e.g. the launcher or IME).

* Read-only `mmap()`ed pages can't be shared between processes using different
versions of the APK.

* On O+ where the WebView can be multiprocess, any newly launched renderer
process would always be using the currently installed APK to provide its
implementation, causing a mismatch between the browser and renderer code.

### Changes in N

For security, we want to verify that the implementation we are loading is
legitimate; on earlier versions the platform took care of this for us because
there was only one implementation package (whose name was configured at build
time), and it was always preinstalled, so could never be replaced by a package
with a different signing key. However, on N, we added support for using the
beta, dev, and canary versions of Chrome as WebView implementations as well, and
since these are not preinstalled, the loading mechanism must validate that they
are signed with the expected keys itself.

Asking `PackageManager` for information about the implementation package,
validating it, and then calling `Context.createPackageContext` would have
created a potentially exploitable time-of-check to time-of-use vulnerability:
since `Context.createPackageContext` just takes a package name as a string, the
package may have been replaced after checking it but before loading it. To avoid
this, we switched to the internal `Context.createApplicationContext` API, which
performs the same function but takes an `ApplicationInfo` object describing the
APK to load instead of a package name. We can then pass in the validated info
and if the package has been replaced in the interim, we will fail to load it,
as the replacement APK will have been installed to a new location by
`PackageManager`.

We also explicitly call `ActivityManager` to notify it that we are using the
package beforehand, which ensures that the process will be killed if the package
is removed or replaced, or if the WebView provider is switched.

## Loading Java code

Loading the Java code is straightforward: the `Context.getClassLoader` method
will give us a `ClassLoader` that refers to the WebView APK, which we can use to
load classes via reflection. The classpath of the classloader should always
contain all relevant things (e.g. APK splits, library APKs) as it's set up by
the same code that is used when the framework loads an APK "normally".

The WebView's classloader is entirely separate to the app's: they are siblings,
with a common parent (the system classloader, which contains the core framework
code). Java classloaders (normally) delegate to their parent before attempting
to load a class themselves, and this means that the app and WebView will always
have the same definition of any framework class. Classes defined in the app APK
are not directly visible to WebView and vice versa.

To avoid having to use reflection throughout the WebView API, we make use of the
fact that the framework is the common parent: the framework defines the WebView
API as a series of interfaces and abstract classes, and the WebView APK defines
classes which implement or subclass them. This still requires one use of
reflection to "bootstrap" the process: `WebViewFactory` must use reflection to
load the class in the WebView APK which implements the framework's
`WebViewFactoryProvider` interface. This interface defines methods that are
used to create (or for singletons, retrieve) instances of all the other required
classes.

* On L - N, the class loaded by reflection is called
`com.android.webview.chromium.WebViewChromiumFactoryProvider`.

* On O+, the class is called
`com.android.webview.chromium.WebViewChromiumFactoryProviderForO` (or `ForP`,
`ForQ`, etc). This was made version-specific to avoid cases where using an
outdated version of WebView on a newer version of Android would crash in
unpredictable ways when new APIs were called - instead, loading will always fail
in a consistent way due to the version-specific provider class being missing.

### Bugs on O in multiprocess mode

On Android O the WebView zygote, used to spawn renderer processes, doesn't get a
full classpath set up as expected: only the base WebView APK and its splits (if
any) are included in the classpath, and any shared library or static shared
library APKs are omitted. WebView did not normally depend on any library APKs in
O, so this doesn't usually cause an issue, but it's one of the reasons why it's
not possible to use Trichrome on O.

In addition, even though the splits are included in the classpath, the WebView
zygote in O stores the classloader object itself into the cache incorrectly if
there are any splits, which causes a duplicate classloader to be created when
a renderer actually starts up; this results in a crash (as it tries to load the
native library twice, which is forbidden). We work around this in the WebView
code by
[using reflection early in initialization](/android_webview/glue/java/src/com/android/webview/chromium/SplitApkWorkaround.java).

## Loading native code with RELRO sharing

Loading the native code in the "usual way" by calling `System.loadLibrary` from
Java would work, as long as the caller was a class in the WebView APK - the JVM
uses the calling class to decide which classloader's native library search path
to use.

However, the WebView's native library has an unusually large `GNU_RELRO`
section. This is the part of the binary which contains data that requires
[relocation](https://en.wikipedia.org/wiki/Relocation_%28computing%29) by the
dynamic linker, but does not need to be writable at runtime. In WebView's case,
this is composed mostly of C++ vtables, as well as other const data structures
that happen to contain pointers to other code and data in the binary, and is
around 2MiB (at time of writing).

We want to avoid each process that uses WebView having a separate copy of the
RELRO section, but since the data depends on the address at which the library
has been loaded, under normal circumstances it can't be shared as different
processes load the library at different addresses. We solve this with a three
step loading process:

### Step 1 - address space reservation and RELRO generation

The system zygote reserves a chunk of address space at boot time, so that all
processes which eventually load WebView's native library can load it at the same
address. This reservation is made by the framework WebView loading code; the
WebView APK is never loaded into the system zygote for security and logistical
reasons.

* On Android L - P, the size of the reservation to use is stored in a persistent
system property, and if the property isn't set, a default of 100MiB is used.
When WebView is updated on a device, the property is set to double the size of
the `.so` file. On 64-bit devices where there's both a 32-bit and 64-bit `.so`
file, the larger of the two (basically always 64-bit) is used to set the size,
and both the 32-bit and 64-bit system zygotes reserve the same amount of space.
This is not very efficient for the 32-bit zygote, where much less space is
typically needed. 2x is used because it's expected that updated libraries may be
larger than the current version (but almost certainly less than 2x), and it's
hard to determine how much virtual address space will be required for a given
library in the first place without parsing the ELF headers.

* On Android Q, the 32-bit system zygote always reserves 130MiB (the typical
amount used on older versions, more than enough space for the 32-bit library),
and the 64-bit system zygote always reserves 1GiB (as address space is virtually
free on 64-bit). The dynamic sizing code was removed as it was complex and did
not handle static shared library APKs correctly - the amount of space required
in practise has not varied a great deal and hardcoded values suffice.

It's theoretically possible (though very unlikely) for the address space
reservation to fail; if this happens then RELRO sharing is simply skipped.

At boot time, and each time that the WebView is updated, a RELRO creator process
is started for each ABI supported by the device. This process loads the WebView
native code to the reserved address with `android_dlopen_ext` and instructs the
linker to write a copy of the RELRO data out to a file after applying the
relocations. This file is stored in a world-readable system directory.

### Step 2 - preloading with RELRO

When an app loads WebView, the loading code attempts to load the WebView native
library with `android_dlopen_ext`, passing in the file containing the
preprepared RELRO data. The linker applies the relocations to the library as
usual, but then checks each relocated 4KiB page against the RELRO file, and any
pages which are identical (typically almost all of them, unless the file is
outdated) are replaced with a read-only mapping from the file. This frees up the
memory used by the relocated pages, as the pages mapped from the file can be
shared. If this load fails for any reason then we simply ignore the error and
continue - the RELRO data will not be shared in this process.

This does not make the JVM aware of the library, and does not call `JNI_OnLoad`;
we are only loading it as a generic native library at this point.

* Android L: The native library must be called `libwebviewchromium.so` and must
have been extracted to disk by `PackageManager` at install time to the normal
location where apps' shared libraries are extracted. Loading the library
directly from the APK is not supported: the system linker only gained the
ability to do this in M, and unlike Chrome, WebView can't (at least practically)
use the Chromium linker to work around it.

* Android M - P: The native library filename is specified by a metadata tag in
the APK's manifest, and can be extracted to disk or loaded directly from the
main WebView APK. It will not be found if it's in a split APK or library APK.
This is one of the reasons why Trichrome can't be used on O or P.

* Android L MR1 - P: In addition to the above requirements, a bug in the dynamic
linker introduced in L MR1 means that the native library must not depend on any
other native libraries unless they are already loaded into the process. If more
than one library is loaded at once, the RELRO sections of the libraries will be
corrupted. This is fixed in Q.

* Android Q: The native library filename is specified by a metadata tag in the
APK's manifest, and can be located anywhere in the normal native library load
path for the WebView classloader, including inside split APKs and library APKs,
as the loading code no longer searches for the library file at all and simply
lets it be found by the linker. Any dependencies of the main native library that
are loaded at the same time are also loaded into the reserved address space and
benefit from RELRO sharing.

### Step 3 - loading with System.loadLibrary

The Java code inside the WebView APK calls `System.loadLibrary` for the main
native library name during initialization. If the library was not already
preloaded in step 2, then the library will actually be loaded by the dynamic
linker now. Once it's loaded (or found), `JNI_OnLoad` will be called to
initialize it, and calls to native methods from Java will work.

Since the default platform library loading mechanism is being used here, there
are no special requirements to enable the library to be found.

Unlike most of the other loading steps, this step is performed by code inside
the WebView APK (in order to call it from the correct classloader context),
which means this step can be changed without changing the framework.

## Loading assets and resources

The WebView APK contains a number of asset files such as Chromium .pak files,
V8 startup snapshots, ICU data, etc. We also have a bunch of Android resources
such as the strings and layouts for the UI surfaces that WebView exposes (e.g.
the date and color pickers used for HTML5 input elements).

Android exposes assets and resources via `AssetManager`, but rather than use the
`AssetManager` associated with the WebView APK `Context` that was created above,
we instead add the WebView APK to the application's own `AssetManager`. This is
important to ensure that resource references all work correctly: it's possible
for WebView resources (like XML layouts) to end up referencing app resources via
themes, and so the resources have to coexist.

In addition, the WebView APK `Context` is not kept around or made available to
the WebView implementation code, so using the app context is the only easy
option at present.

The `AssetManager` maintains two main pieces of state that are relevant: the
asset path, which is simply a list of all the APKs that it's managing, and a
mapping from package IDs to those APKs (explained further below).

* Android L-P: Only the base WebView APK is added to the application's asset
path. Assets and resources in split APKs or library APKs will not be usable.
This is one of the reasons why Trichrome can't be used on O or P, and also means
that all of WebView's assets and resources must be in the base module when using
bundles.

* Android Q: All the APKs in the WebView's classpath are added to the
application's asset path.

### Assets

Assets are identified simply by their filename, which can include subdirectory
paths, and they're searched for in the `assets` directory of every APK in the
asset path. Since WebView's assets have been added to the app's asset path, we
can find and load them using the `AssetManager` obtained from the app context.

One complication here is that because the asset path in use contains both the
WebView APK and the app APK, it's possible for asset filenames to collide. It
appears that in this case, WebView's assets take priority, which mean that our
assets work as expected, but the app will get the wrong files and may break.
This is a particular problem for any app that includes Chromium code while also
using the system WebView, since filename collisions are highly likely.

### Resources

Resources are usually identified by a 32-bit ID, though it is also possible to
look them up by name using
[`Resources.getIdentifier`](https://developer.android.com/reference/android/content/res/Resources#getIdentifier%28java.lang.String,%20java.lang.String,%20java.lang.String%29)
at a cost to performance. The bottom three bytes of the ID identify the specific
resource (one byte type, two byte index), but the top byte of this ID is the
"package ID" assigned by the `AssetManager`. The app's own resources are always
assigned the package ID `0x7f`, and the framework's resources are always
assigned the package ID `0x01`. 0 is invalid, and the values between `0x02` and
`0x7e` are dynamically allocated for library APKs.

When WebView is added to the application's `AssetManager`, it's assigned the
next free package ID, which is typically (but not always) `0x02`. This means
that unlike assets, WebView's resources cannot conflict with the app's; but it
also means that WebView doesn't know the actual ID of its resources at compile
time.

* Android L-M: The WebView APK must have been built using the `--shared-lib`
flag for `aapt`. This flag allows the resource IDs to be assigned at runtime,
instead of assuming that they will begin with `0x7f`. It also makes the
generated R class fields non-final, so that they can be patched at runtime to
have the correct package ID. However, an APK built with this flag will not be
able to use its resources in the normal way if it's launched as an app, instead
of being loaded as a shared library.

* Android N-Q: The WebView APK can be built as on L-M, but can also be built
using the `--app-as-shared-lib` flag instead. This allows the resource IDs to
be assigned at runtime and makes the R class fields non-final as above, but also
makes it possible to load the APK as a regular app with an `0x7f` package ID.
This is what makes Monochrome work, as with the old approach Chrome would not be
able to find its own resources when launched normally. APKs built this way are
not compatible with older OS versions.

WebView is usually loaded as a library and so both modes work, but we do have
some cases where we launch it as a regular app such as the open source license
viewer and the service used to send crash reports. To ensure that these use
cases work correctly on L and M, it's necessary to look up resources by name
using the framework APIs instead of relying on resource IDs.

### Updating the asset path to work around app incompatibilities

A number of applications do unusual things with contexts, resources, and assets.
Android originally exposed the APIs required for apps to construct their own
instances of `Resources` and to change the `Configuration` being used. These
were not intended to be public and are now deprecated, but many apps still rely
on them. Some apps simply override context methods like `getAssets` and
`getResources`, and do not always maintain the platform invariants.

This sometimes means that the application-level `Context` to which WebView's
APK was added during initialization and the `Context` that a given instance of
WebView is actually using do not share the same `AssetManager` as we expect,
and as a result, WebView may fail to find its resources when using the
instance-specific context. We can't simply always use the app context, because
different contexts may have different themes or configurations that may result
in (correctly) resolving resources differently.

So, to partially work around this, versions of WebView since L MR1 also try to
add the WebView APK to the asset path of the instance-specific context when a
WebView instance is constructed. This is usually a no-op as it's already present
from the app context, but helps some apps work correctly.

Unfortunately this mechanism is not 100% effective and some application usage
patterns relying on deprecated platform APIs still result in a failure to find
WebView resources at runtime, and there's not much we can do about this.

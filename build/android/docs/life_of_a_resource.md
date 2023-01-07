# Life of an Android Resource

[TOC]

## Overview

This document describes how [Android Resources][android resources]
are built in Chromium's build system. It does not mention native resources
which are [processed differently][native resources].

[android resources]: https://developer.android.com/guide/topics/resources/providing-resources
[native resources]: https://www.chromium.org/developers/tools-we-use-in-chromium/grit/grit-users-guide

The steps consume the following files as inputs:
* `AndroidManifest.xml`
  * Including `AndroidManifest.xml` files from libraries, which get merged
    together
* res/ directories

The steps produce the following intermediate files:
* `R.srcjar` (contains `R.java` files)
* `R.txt`
* `.resources.zip`

The steps produce the following files within an `.apk`:
* `AndroidManifest.xml` (a binary xml file)
* `resources.arsc` (contains all values and configuration metadata)
* `res/**` (drawables and layouts)
* `classes.dex` (just a small portion of classes from generated `R.java` files)


## The Build Steps

Whenever you try to compile an apk or library target, resources go through the
following steps:

### 1. Constructs .build\_config files:

Inputs:
* GN target metadata
* Other `.build_config.json` files

Outputs:
* Target-specific `.build_config.json` file

`write_build_config.py` is run to record target metadata needed by future steps.
For more details, see [build_config.md](build_config.md).


### 2. Prepares resources:

Inputs:
* Target-specific `.build_config.json` file
* Files listed as `sources`

Outputs:
* Target-specific `resources.zip` (contains all resources listed in `sources`).
* Target-specific `R.txt` (list of all resources, including dependencies).

`prepare_resources.py` zips up the target-specific resource files and generates
`R.txt`. No optimizations, crunching, etc are done on the resources.

**The following steps apply only to apk & bundle targets (not to library
targets).**

### 3. Create target-specific R.java files

Inputs:
* `R.txt` from dependencies.

Outputs:
* Target-specific (placeholder) `R.java` file.

A target-specific `R.java` is generated for each `android_library()` target that
sets `resources_package`. Resource IDs are not known at this phase, so all
values are set as placeholders. This copy of `R` classes are discarded and
replaced with new copies at step 4.

Example placeholder R.java file:
```java
package org.chromium.mypackage;

public final class R {
    public static class anim  {
        public static int abc_fade_in = 0;
        public static int abc_fade_out = 0;
        ...
    }
    ...
}
```

### 4. Finalizes apk resources:

Inputs:
* Target-specific `.build_config.json` file
* Dependencies' `R.txt` files
* Dependencies' `resources.zip` files

Output:
* Packaged `resources zip` (named `foo.ap_`) containing:
  * `AndroidManifest.xml` (as binary xml)
  * `resources.arsc`
  * `res/**`
* Final `R.txt`
  * Contains a list of resources and their ids (including of dependencies).
* Final `R.java` files
  * See [What are `R.java` files and how are they generated](
  #how-r_java-files-are-generated)


#### 4(a). Compiles resources:

For each library / resources target your apk depends on, the following happens:
* Use a regex (defined in the apk target) to remove select resources (optional).
* Convert png images to webp for binary size (optional).
* Move drawables in mdpi to non-mdpi directory ([why?](http://crbug.com/289843))
* Use `aapt2 compile` to compile xml resources to binary xml (references to
  other resources will now use the id rather than the name for faster lookup at
  runtime).
* `aapt2 compile` adds headers/metadata to 9-patch images about which parts of
  the image are stretchable vs static.
* `aapt2 compile` outputs a zip with the compiled resources (one for each
  dependency).


#### 4(b). Links resources:

After each dependency is compiled into an intermediate `.zip`, all those zips
are linked by the `aapt2 link` command which does the following:
* Use the order of dependencies supplied so that some resources clober each
  other.
* Compile the `AndroidManifest.xml` to binary xml (references to resources are
  now using ids rather than the string names)
* Create a `resources.arsc` file that has the name and values of string
  resources as well as the name and path of non-string resources (ie. layouts
  and drawables).
* Combine the compiled resources into one packaged resources apk (a zip file
  with an `.ap_` extension) that has all the resources related files.


#### 4(c). Optimizes resources:

Targets can opt into the following optimizations:
1) Resource name collapsing: Maps all resources to the same name. Access to
   resources via `Resources.getIdentifier()` no longer work unless resources are
   [allowlisted](#adding-resources-to-the-allowlist).
2) Resource filename obfuscation: Renames resource file paths from e.g.:
   `res/drawable/something.png` to `res/a`. Rename mapping is stored alongside
   APKs / bundles in a `.pathmap` file. Renames are based on hashes, and so are
   stable between builds (unless a new hash collision occurs).
3) Unused resource removal: Referenced resources are extracted from the
   optimized `.dex` and `AndroidManifest.xml`. Resources that are directly or
   indirectly used by these files are removed.

## App Bundles and Modules:

Processing resources for bundles and modules is slightly different. Each module
has its resources compiled and linked separately (ie: it goes through the
entire process for each module). The modules are then combined to form a
bundle. Moreover, during "Finalizing the apk resources" step, bundle modules
produce a `resources.proto` file instead of a `resources.arsc` file.

Resources in a dynamic feature module may reference resources in the base
module. During the link step for feature module resources, the linked resources
of the base module are passed in. However, linking against resources currently
works only with `resources.arsc` format. Thus, when building the base module,
resources are compiled as both `resources.arsc` and `resources.proto`.

## Debugging resource related errors when resource names are obfuscated

An example message from a stacktrace could be something like this:
```
java.lang.IllegalStateException: Could not find CoordinatorLayout descendant
view with id org.chromium.chrome:id/0_resource_name_obfuscated to anchor view
android.view.ViewStub{be192d5 G.E...... ......I. 0,0-0,0 #7f0a02ad
app:id/0_resource_name_obfuscated}
```

`0_resource_name_obfuscated` is the resource name for all resources that had
their name obfuscated/stripped during the optimize resources step. To help with
debugging, the `R.txt` file is archived. The `R.txt` file contains a mapping
from resource ids to resource names and can be used to get the original resource
name from the id. In the above message the id is `0x7f0a02ad`.

For local builds, `R.txt` files are output in the `out/*/apks` directory.

For official builds, Googlers can get archived `R.txt` files next to archived
apks.

### Adding resources to the allowlist

If a resource is accessed via `getIdentifier()` it needs to be allowed by an
aapt2 resources config file. The config file looks like this:

```
<resource type>/<resource name>#no_obfuscate
```
eg:
```
string/app_name#no_obfuscate
id/toolbar#no_obfuscate
```

The aapt2 config file is passed to the ninja target through the
`resources_config_paths` variable. To add a resource to the allowlist, check
where the config is for your target and add a new line for your resource. If
none exist, create a new config file and pass its path in your target.

### Webview resource ids

The first two bytes of a resource id is the package id. For regular apks, this
is `0x7f`. However, Webview is a shared library which gets loaded into other
apks. The package id for webview resources is assigned dynamically at runtime.
When webview is loaded it calls this [R file's][Base Module R.java File]
`onResourcesLoaded()` function to have the correct package id. When
deobfuscating webview resource ids, disregard the first two bytes in the id when
looking it up in the `R.txt` file.

Monochrome, when loaded as webview, rewrites the package ids of resources used
by the webview portion to the correct value at runtime, otherwise, its resources
have package id `0x7f` when run as a regular apk.

[Base Module R.java File]: https://cs.chromium.org/chromium/src/out/android-Debug/gen/android_webview/system_webview_apk/generated_java/gen/base_module/R.java

## How R.java files are generated

`R.java` contain a set of nested static classes, each with static fields
containing ids. These ids are used in java code to reference resources in
the apk.

There are three types of `R.java` files in Chrome.
1. Root / Base Module `R.java` Files
2. DFM `R.java` Files
3. Per-Library `R.java` Files

### Root / Base Module `R.java` Files
Contain base android resources. All `R.java` files can access base module
resources through inheritance.

Example Root / Base Module `R.java` File:
```java
package gen.base_module;

public final class R {
    public static class anim  {
        public static final int abc_fade_in = 0x7f010000;
        public static final int abc_fade_out = 0x7f010001;
        public static final int abc_slide_in_top = 0x7f010007;
    }
    public static class animator  {
        public static final int design_appbar_state_list_animator = 0x7f020000;
    }
}
```

### DFM `R.java` Files
Extend base module root `R.java` files. This allows DFMs to access their own
resources as well as the base module's resources.

Example DFM Root `R.java` File
```java
package gen.vr_module;

public final class R {
    public static class anim extends gen.base_module.R.anim {
    }
    public static class animator extends gen.base_module.R.animator  {
        public static final int design_appbar_state_list_animator = 0x7f030000;
    }
}
```

### Per-Library `R.java` Files
Generated for each `android_library()` target that sets `resources_package`.
First a placeholder copy is generated in the `android_library()` step, and then
a final copy is created during finalization.

Example final per-library `R.java`:
```java
package org.chromium.chrome.vr;

public final class R {
    public static final class anim extends
            gen.vr_module.R.anim {}
    public static final class animator extends
            gen.vr_module.R.animator {}
}
```

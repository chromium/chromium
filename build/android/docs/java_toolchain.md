# Chromium's Java Toolchain

This doc aims to describe the Chrome build process that takes a set of `.java`
files and turns them into a `classes.dex` file.

[TOC]

## Core GN Target Types

The following have `supports_android` and `requires_android` set to false by
default:
* `java_library()`: Compiles `.java` -> `.jar`
* `java_prebuilt()`:  Imports a prebuilt `.jar` file.

The following have `supports_android` and `requires_android` set to true. They
also have a default `jar_excluded_patterns` set (more on that later):
* `android_library()`
* `android_java_prebuilt()`

All target names must end with "_java" so that the build system can distinguish
them from non-java targets (or [other variations](https://cs.chromium.org/chromium/src/build/config/android/internal_rules.gni?rcl=ec2c17d7b4e424e060c3c7972842af87343526a1&l=20)).

Most targets produce two separate `.jar` files:
* Device `.jar`: Used to produce `.dex.jar`, which is used on-device.
* Host `.jar`: For use on the host machine (`junit_binary` / `java_binary`).
  * Host `.jar` files live in `lib.java/` so that they are archived in
    builder/tester bots (which do not archive `obj/`).

## From Source to Final Dex

### Step 1: Create interface .jar with turbine or ijar

What are interface jars?:

* They contain `.class` files with all private symbols and all method bodies
  removed.
* Dependant targets use interface `.jar` files to skip having to be rebuilt
  when only private implementation details change.

For prebuilt `.jar` files: we use [//third_party/ijar] to create interface
`.jar` files from the prebuilt ones.

For non-prebuilt `.jar` files`: we use [//third_party/turbine] to create
interface `.jar` files directly from `.java` source files. Turbine is faster
than javac because it does not compile method bodies. Although Turbine causes
us to compile files twice, it speeds up builds by allowing `javac` compilation
of targets to happen concurrently with their dependencies. We also use Turbine
to run our annotation processors.

[//third_party/ijar]: /third_party/ijar/README.chromium
[//third_party/turbine]: /third_party/turbine/README.chromium

### Step 2a: Compile with javac

This step is the only step that does not apply to prebuilt targets.

* All `.java` files in a target are compiled by `javac` into `.class` files.
  * This includes `.java` files that live within `.srcjar` files, referenced
    through `srcjar_deps`.
* The `classpath` used when compiling a target is comprised of `.jar` files of
  its deps.
  * When deps are library targets, the Step 1 `.jar` file is used.
  * When deps are prebuilt targets, the original `.jar` file is used.
  * All `.jar` processing done in subsequent steps does not impact compilation
    classpath.
* `.class` files are zipped into an output `.jar` file.
* There is **no support** for incremental compilation at this level.
  * If one source file changes within a library, then the entire library is
    recompiled.
  * Prefer smaller targets to avoid slow compiles.

### Step 2b: Compile with ErrorProne

This step can be disabled via GN arg: `use_errorprone_java_compiler = false`

* Concurrently with step 1a: [ErrorProne] compiles java files and checks for bug
  patterns, including some [custom to Chromium][ep_plugins].
* ErrorProne used to replace step 1a, but was changed to a concurrent step after
  being identified as being slower.

[ErrorProne]: https://errorprone.info/
[ep_plugins]: /tools/android/errorprone_plugin/

### Step 3: Desugaring (Device .jar Only)

This step happens only when targets have `supports_android = true`. It is not
applied to `.jar` files used by `junit_binary`.

* `//third_party/bazel/desugar` converts certain Java 8 constructs, such as
  lambdas and default interface methods, into constructs that are compatible
  with Java 7.

### Step 4: Instrumenting (Device .jar Only)

This step happens only when this GN arg is set: `use_jacoco_coverage = true`

* [Jacoco] adds instrumentation hooks to methods.

[Jacoco]: https://www.eclemma.org/jacoco/

### Step 5: Filtering

This step happens only when targets that have `jar_excluded_patterns` or
`jar_included_patterns` set (e.g. all `android_` targets).

* Remove `.class` files that match the filters from the `.jar`. These `.class`
  files are generally those that are re-created with different implementations
  further on in the build process.
  * E.g.: `R.class` files - a part of [Android Resources].
  * E.g.: `GEN_JNI.class` - a part of our [JNI] glue.

[JNI]: /third_party/jni_zero/README.md
[Android Resources]: life_of_a_resource.md

### Step 6: Per-Library Dexing

This step happens only when targets have `supports_android = true`.

* [d8] converts `.jar` files containing `.class` files into `.dex.jar` files
  containing `classes.dex` files.
* Dexing is incremental - it will reuse dex'ed classes from a previous build if
  the corresponding `.class` file is unchanged.
* These per-library `.dex.jar` files are used directly by [incremental install],
  and are inputs to the Apk step when `enable_proguard = false`.
  * Even when `is_java_debug = false`, many apk targets do not enable ProGuard
    (e.g. unit tests).

[d8]: https://developer.android.com/studio/command-line/d8
[incremental install]: /build/android/incremental_install/README.md

### Step 7: Apk / Bundle Module Compile

* Each `android_apk` and `android_bundle_module` template has a nested
  `java_library` target. The nested library includes final copies of files
  stripped out by prior filtering steps. These files include:
  * Final `R.java` files, created by `compile_resources.py`.
  * Final `GEN_JNI.java` for [JNI glue].
  * `BuildConfig.java` and `NativeLibraries.java` (//base dependencies).

[JNI glue]: /third_party/jni_zero/README.md

### Step 8: Final Dexing

This step is skipped when building using [Incremental Install].

When `is_java_debug = true`:
* [d8] merges all library `.dex.jar` files into a final `.mergeddex.jar`.

When `is_java_debug = false`:
* [R8] performs whole-program optimization on all library `lib.java` `.jar`
  files and outputs a final `.r8dex.jar`.
  * For App Bundles, R8 creates a `.r8dex.jar` for each module.

[Incremental Install]: /build/android/incremental_install/README.md
[R8]: https://r8.googlesource.com/r8

## Test APKs with apk_under_test

Test APKs are normal APKs that contain an `<instrumentation>` tag within their
`AndroidManifest.xml`. If this tag specifies an `android:targetPackage`
different from itself, then Android will add that package's `classes.dex` to the
test APK's Java classpath when run. In GN, you can enable this behavior using
the `apk_under_test` parameter on `instrumentation_test_apk` targets. Using it
is discouraged if APKs have `proguard_enabled=true`.

### Difference in Final Dex

When `enable_proguard=false`:
* Any library depended on by the test APK that is also depended on by the
  apk-under-test is excluded from the test APK's final dex step.

When `enable_proguard=true`:
* Test APKs cannot make use of the apk-under-test's dex because only symbols
  explicitly kept by `-keep` directives are guaranteed to exist after
  ProGuarding. As a work-around, test APKs include all of the apk-under-test's
  libraries directly in its own final dex such that the under-test apk's Java
  code is never used (because it is entirely shadowed by the test apk's dex).
  * We've found this configuration to be fragile, and are trying to [move away
    from it](https://bugs.chromium.org/p/chromium/issues/detail?id=890452).

### Difference in GEN_JNI.java
* Calling native methods using [JNI glue] requires that a `GEN_JNI.java` class
  be generated that contains all native methods for an APK. There cannot be
  conflicting `GEN_JNI` classes in both the test apk and the apk-under-test, so
  only the apk-under-test has one generated for it. As a result this,
  instrumentation test APKs that use apk-under-test cannot use native methods
  that aren't already part of the apk-under-test.

## How to Generate Java Source Code
There are two ways to go about generating source files: Annotation Processors
and custom build steps.

### Annotation Processors
* These are run by `javac` as part of the compile step.
* They **cannot** modify the source files that they apply to. They can only
  generate new sources.
* Use these when:
  * an existing Annotation Processor does what you want
    (E.g. Dagger, AutoService, etc.), or
  * you need to understand Java types to do generation.

### Custom Build Steps
* These use discrete build actions to generate source files.
  * Some generate `.java` directly, but most generate a zip file of sources
    (called a `.srcjar`) to simplify the number of inputs / outputs.
* Examples of existing templates:
  * `jinja_template`: Generates source files using [Jinja].
  * `java_cpp_template`: Generates source files using the C preprocessor.
  * `java_cpp_enum`: Generates `@IntDef`s based on enums within `.h` files.
  * `java_cpp_strings`: Generates String constants based on strings defined in
    `.cc` files.
* Custom build steps are preferred over Annotation Processors because they are
  generally easier to understand, and can run in parallel with other steps
  (rather than being tied to compiles).

[Jinja]: https://palletsprojects.com/p/jinja/

## Static Analysis & Code Checks

See [static_analysis.md](static_analysis.md)

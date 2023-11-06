# Static Analysis

We use several tools for static analysis in chromium.

[TOC]

## [Android Lint](lint.md)
* Runs as part of normal compilation.
* Controlled by GN arg: `disable_android_lint` (or `android_static_analysis`).
* [Useful checks include](https://googlesamples.github.io/android-custom-lint-rules/checks/index.md.html):
  * `NewApi` (ensureing `Build.VERSION.SDK_INT` checks are in place).
* A list of disabled checks is found [within `lint.py`].
  * and [`lint-baseline.xml`] files contain individual suppressions.
* Custom lint checks [are possible], but we don't have any.
* Checks run on the entire codebase, not only on changed lines.
* Does not run when `chromium_code = false` (e.g. for `//third_party`).

[are possible]: https://googlesamples.github.io/android-custom-lint-rules/api-guide.md.html
[within `lint.py`]: https://source.chromium.org/chromium/chromium/src/+/main:build/android/gyp/lint.py;l=25
[`lint-baseline.xml`]: https://source.chromium.org/search?q=f:lint-baseline.xml%20-f:third_party

## [ErrorProne](https://errorprone.info/)
* Runs as part of normal compilation.
* Controlled by GN arg: `use_errorprone_java_compiler` (or
  `android_static_analysis`).
* [Useful checks include](https://errorprone.info/bugpatterns):
  * Enforcement of `@GuardedBy`, `@CheckReturnValue`, and `@DoNotMock`.
  * Enforcement of `/* paramName= */` comments.
* A list of enabled / disabled checks is found [within `compile_java.py`](https://cs.chromium.org/chromium/src/build/android/gyp/compile_java.py?l=30)
  * Many checks are currently disabled because there is work involved in fixing
    violations they introduce. Please help!
* Chrome has [a few custom checks]:
* Checks run on the entire codebase, not only on changed lines.
* Does not run when `chromium_code = false` (e.g. for `//third_party`).

[a few custom checks]: /tools/android/errorprone_plugin/src/org/chromium/tools/errorprone/plugin/

## [Checkstyle](https://checkstyle.sourceforge.io/)
* Mainly used for checking Java formatting & style.
  * E.g.: Unused imports and naming conventions.
* Allows custom checks to be added via XML. Here [is ours].
* Preferred over adding checks via `PRESUBMIT.py` because the tool understands
  `@SuppressWarnings` annotations.
* Runs only on changed lines as a part of `PRESUBMIT.py`.

[is ours]:  /tools/android/checkstyle/chromium-style-5.0.xml

## [PRESUBMIT.py](/PRESUBMIT.py):
* Checks for banned patterns via `_BANNED_JAVA_FUNCTIONS`.
  * (These should likely be moved to checkstyle).
* Checks for a random set of things in `ChecksAndroidSpecificOnUpload()`.
  * Including running Checkstyle.
* Runs only on changed lines.

## [Bytecode Processor](/build/android/bytecode/)
* Runs as part of normal compilation.
* Controlled by GN arg: `android_static_analysis`.
* Performs a single check:
  * Enforces that targets do not rely on indirect dependencies to populate
    their classpath.
  * In other words: that `deps` are not missing any entries.

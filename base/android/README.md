# //base/android

This directory contains:

* C++ APIs that are broadly useful and are unique to `target_os="android"`, and
* Java APIs that are broadly useful, along C++ bindings when necessary.

This directory does not contain Android-specific implementations / extensions
to APIs declared directly in `//base`. Those live in `//base/*_android.cc`, or
behind `#ifdef`s.

## Adding New APIs

The advice laid out in [//base/README.md] applies to this directory as well.
The bars for what APIs should exist and for code quality are generally higher
than for other directories. If you find yourself wanting to add a new API, you
should expect that code reviews take multiple revisions and that they be met
with (respectful) scrutiny.

If you are not sure whether an API would make sense to add, you can ask via
java@chromium.org.
It is common to add APIs to `//chrome` (or elsewhere) first, and move them into
`//base` after their usefulness has been proven.

[//base/README.md]: /base/README.md

### What Uses //base/android?

The main two clients are Chrome and WebView, but it is also used by other
Chromium-based apps, such as Chromecast and Chrome Remote desktop. Some
`//base/android` classes are used by `//build` (this is a layering violation,
tracked in [crbug/1364192] and [crbug/1377351]).

Two considerations for WebView:

1. The application Context is that of the host app's.
2. The UI thread might be different from the main thread.

[crbug/1364192]: https://crbug.com/1364192
[crbug/1377351]: https://crbug.com/1377351


### New API Checklist

Here is a list of checks you should go through when adding a new API:

1. The functionality does not already exist in system libraries (Java APIs,
   Android SDK) or in already adopted `third_party` libraries, such as AndroidX.
2. Reasonable effort has been made to ensure the new API is discoverable. E.g.:
   Coordinate refactorings of existing patterns to it, add a [presubmit check],
   to recommend it, etc.
3. Tests (ideally Robolectric) are added.
4. Thought has been put into API design.
   * E.g. adding `@Nullable`, or `@DoNotMock`
   * E.g. adding test helpers, such as `ForTesting()` methods or `TestRule`s
   * E.g. adding asserts or comments about thread-safety
   * E.g. could usage of the API be made harder to get wrong?

[presumbit check]: https://chromium.googlesource.com/chromium/src/+/main/build/android/docs/java_toolchain.md#Static-Analysis-Code-Checks

### Choosing a Reviewer

All members of [`//base/android/OWNERS`] will be CC'ed on reviews through a
[`//WATCHLIST`] entry. For new APIs, feel free to pick a reviewer at random.
For modifying existing files, it is best to use a reviewer from prior changes to
the file.

[`//base/android/OWNERS`]: /base/android/OWNERS
[`//WATCHLIST`]: /WATCHLIST

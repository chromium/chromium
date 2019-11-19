# Public Chrome on Android Targets

This is the top-level directory for all chrome on android targets that do not
depend on files under `//chrome/android/java` or `//chrome/android/features`.
Each subdirectory should specify its own java target and contain all the
source code that target defines. e.g. [lifecycle/BUILD.gn](lifecycle/BUILD.gn).

This layer can be considered as the base layer within the android-specific
chrome layer. This directory serves a similar purpose as the `public/`
subdirectories within individual features under `//chrome/android/features`.
Those features can then safely depend on targets in `//chrome/android/public`
without needing to transitively depending on all of the code in
`//chrome/android/java`.

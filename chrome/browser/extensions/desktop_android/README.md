This directory represents code specific to the experimental desktop android
extensions platform, tracked in https://crbug.com/356905053.

The majority of code in this directory should be considered *temporary*.
Desktop-android should leverage the same code as other desktop platforms in
common cases. This code is only necessary while we continue refactoring to
allow more pieces of the extensions system to compile on desktop-android.
During this refactoring period, it is valuable to have functional runtime where
we exercise and test extension capabilities; this directory provides those
components.

Code should only be added here if it is either:
a) intentionally temporary and only meant to last during this experimental
   phase, or
b) code which necessarily needs to differ between desktop-android and other
   desktop platforms.

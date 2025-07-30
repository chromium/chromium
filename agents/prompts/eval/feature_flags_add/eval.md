* Owner: wenyufu@chromium.org
* Description: Add a new feature flag with name in the given feature list following `//docs/how_to_add_your_feature_flag.md`
* Git-Revision: Ib672ab0fbe1b85521e7142ca2cb2881ef9b6b034
* Result:
  * Feature flag "AndroidResourceProvider" being added for chrome_feature_list.
  * A flag entry in about_flags.cc are added.
* Modified files:

```
chrome/browser/about_flags.cc
chrome/browser/flag_descriptions.h
chrome/browser/flag_descriptions.cc
chrome/browser/flag-metadata.json
chrome/browser/flags/android/chrome_feature_list.h
chrome/browser/flags/android/chrome_feature_list.cc
chrome/browser/flags/android/java/src/org/chromium/chrome/browser/flags/ChromeFeatureList.java
tools/metrics/histograms/enums.xml
```

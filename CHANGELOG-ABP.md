# Release Notes
## Chromium 68.0
* Updated to Chromium `68.0.3440.70`
* [Use Chromium Android NDK](https://gitlab.com/eyeo/adblockplus/chromium/issues/26) when building, instead of downloading official Google Android NDK
* [Fixed a bug](https://issues.adblockplus.org/ticket/6799) where Hebrew subscription was not installed for Hebrew locale

## Chromium 67.0
* Updated to Chromium `67.0.3396.87`
* [Update](https://gitlab.com/eyeo/adblockplus/chromium/issues/7) the way `libadblockplus` and `libadblockplus-android` are built
* [Added support](https://issues.adblockplus.org/ticket/6632) for:
   - `am-ET - Amharic (Ethiopia)`
   - `fr-CA - French (Canada)`
   - `fil-PH - Filipino (Philippines)`
   - `sw-KE - Kiswahili (Kenya)`
* [Initialize](https://gitlab.com/eyeo/adblockplus/chromium/issues/9) filter engine asynchronously, and [wait](https://gitlab.com/eyeo/adblockplus/chromium/issues/14) for it in Settings.
* Stop using [deprecated libadblockplus-android API](https://gitlab.com/eyeo/adblockplus/chromium/issues/13)
* [Disable](https://gitlab.com/eyeo/adblockplus/chromium/issues/20) Chromium's built-in ad blocking
* [Support](https://gitlab.com/eyeo/adblockplus/chromium/issues/22) configuring of android package name filter list network requests 
* [Correctly detect](https://gitlab.com/eyeo/adblockplus/chromium/issues/24) the type of image resources
* Added a requirement to have 7zip installed on building machine

[Full list of `libadblockplus` changes](https://github.com/adblockplus/libadblockplus/compare/ea5309a0a6f3c5ab1e378b79c09d930ac3fbcfd0...40f5d4d2d00abe3f94ce69210267bcce908cd748).

[Full list of `libadblockplus-android` changes](https://github.com/adblockplus/libadblockplus-android/compare/7ef63f2b8458a5b23595bd22c180c0e6f2398801...5342ea7697a7a55a3f649aadb6a976a0799e7922)

[Full list of `adblockpluschromium` changes](https://github.com/adblockplus/chromium/compare/483c3b509b0f032a7e92bd4fce339e70c452d2aa...6b18f01e06da9bcb1f5ffdd3c9cfb1e1e485cbf2)

## Chromium 65.0
* Base release of ad blocking integration

# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Different build variants of chrome for android have different version codes.
Reason: for targets that have the same package name (e.g. chrome, chome
modern, monochrome, trichrome), Play Store considers them the same app
and will push the supported app with the highest version code to devices.
(note Play Store does not support hosting two different apps with same
version code and package name)

Each key in this dict represents a unique version code that will be used for
one or more android chrome apks.

Webview channels must have unique version codes for a couple reasons:
a) Play Store does not support having the same version code for different
   versions of a package. Without unique codes, promoting a beta apk to stable
   would require first removing the beta version.
b) Firebase project support (used by official builders) requires unique
   [version code + package name].
   We cannot add new webview package names for new channels because webview
   packages are whitelisted by Android as webview providers.

WEBVIEW_STABLE, WEBVIEW_BETA, WEBVIEW_DEV are all used for standalone webview,
whereas the others are used for various chrome apks.

Note that a final digit of '3' for webview is reserved for Trichrome Webview.
The same versionCode is used for both Trichrome Chrome and Trichrome Webview.
"""
ANDROID_CHROME_APK_VERSION_CODE_DIFFS = {
    'CHROME': 0,
    'CHROME_MODERN': 1,
    'MONOCHROME': 2,
    'TRICHROME': 3,
    'NOTOUCH_CHROME': 4,
    'WEBVIEW_STABLE': 0,
    'WEBVIEW_BETA': 1,
    'WEBVIEW_DEV': 2,
}

"""The architecture preference is encoded into the version_code for devices
that support multiple architectures. (exploiting play store logic that pushes
apk with highest version code)

Detail:
Many Android devices support multiple architectures, and can run applications
built for any of them; the Play Store considers all of the supported
architectures compatible and does not, itself, have any preference for which
is "better". The common cases here:

- All production arm64 devices can also run arm
- All production x64 devices can also run x86
- Pretty much all production x86/x64 devices can also run arm (via a binary
  translator)

Since the Play Store has no particular preferences, you have to encode your own
preferences into the ordering of the version codes. There's a few relevant
things here:

- For any android app, it's theoretically preferable to ship a 64-bit version to
  64-bit devices if it exists, because the 64-bit architectures are supposed to
  be "better" than their 32-bit predecessors (unfortunately this is not always
  true due to the effect on memory usage, but we currently deal with this by
  simply not shipping a 64-bit version *at all* on the configurations where we
  want the 32-bit version to be used).
- For any android app, it's definitely preferable to ship an x86 version to x86
  devices if it exists instead of an arm version, because running things through
  the binary translator is a performance hit.
- For WebView, Monochrome, and Trichrome specifically, they are a special class
  of APK called "multiarch" which means that they actually need to *use* more
  than one architecture at runtime (rather than simply being compatible with
  more than one). The 64-bit builds of these multiarch APKs contain both 32-bit
  and 64-bit code, so that Webview is available for both ABIs. If you're
  multiarch you *must* have a version that supports both 32-bit and 64-bit
  version on a 64-bit device, otherwise it won't work properly. So, the 64-bit
  version needs to be a higher versionCode, as otherwise a 64-bit device would
  prefer the 32-bit version that does not include any 64-bit code, and fail.
- The relative order of mips isn't important, but it needs to be a *distinct*
  value to the other architectures because all builds need unique version codes.
"""
ARCH_VERSION_CODE_DIFF = {
    'arm': 0,
    'x86': 10,
    'mipsel': 20,
    'arm64': 30,
    'x64': 60
}
ARCH_CHOICES = ARCH_VERSION_CODE_DIFF.keys()

""" "Next" builds get +5 last version code digit.

We choose 5 because it won't conflict with values in
ANDROID_CHROME_APK_VERSION_CODE_DIFFS
"""
NEXT_BUILD_VERSION_CODE_DIFF = 5

"""For 64-bit architectures, some packages have multiple targets with version
codes that differ by the second-to-last digit (the architecture digit). This is
for various combinations of 32-bit vs 64-bit chrome and webview. The
default/traditional configuration is 32-bit chrome with 64-bit webview, but we
are adding:
+ 64-bit chrome with 32-bit webview
+ 64-bit combined Chrome and Webview (only one library)
+ (maybe someday 32-bit chrome with 32-bit webview)

The naming scheme followed here is <chrome>_<webview>,
e.g. 64_32 is 64-bit chrome with 32-bit webview.
"""
ARCH64_APK_VARIANTS = {
    '64_32': {
        'PACKAGES': frozenset(['MONOCHROME', 'TRICHROME']),
        'MODIFIER': 10
    },
    '64': {
        'PACKAGES': frozenset(['MONOCHROME', 'TRICHROME']),
        'MODIFIER': 20
    }
}


def GenerateVersionCodes(version_values, arch, is_next_build):
  """Get dict of version codes for chrome-for-android-related targets

  e.g.
  {
    'CHROME_VERSION_CODE': '378100010',
    'MONOCHROME_VERSION_CODE': '378100013',
    ...
  }

  versionCode values are built like this:
  {full BUILD int}{3 digits for PATCH}{1 digit for architecture}{final digit}.

  MAJOR and MINOR values are not used for generating versionCode.
  - MINOR is always 0. It was used for something long ago in Chrome's history
    but has not been used since, and has never been nonzero on Android.
  - MAJOR is cosmetic and controlled by the release managers. MAJOR and BUILD
    always have reasonable sort ordering: for two version codes A and B, it's
    always the case that (A.MAJOR < B.MAJOR) implies (A.BUILD < B.BUILD), and
    that (A.MAJOR > B.MAJOR) implies (A.BUILD > B.BUILD). This property is just
    maintained by the humans who set MAJOR.

  Thus, this method is responsible for the final two digits of versionCode.
  """

  base_version_code = '%s%03d00' % (version_values['BUILD'],
                                    int(version_values['PATCH']))
  new_version_code = int(base_version_code)

  new_version_code += ARCH_VERSION_CODE_DIFF[arch]
  if is_next_build:
    new_version_code += NEXT_BUILD_VERSION_CODE_DIFF

  version_codes = {}
  for apk, diff in ANDROID_CHROME_APK_VERSION_CODE_DIFFS.iteritems():
    version_code_name = apk + '_VERSION_CODE'
    version_code_val = new_version_code + diff
    version_codes[version_code_name] = str(version_code_val)

    for variant, config in ARCH64_APK_VARIANTS.iteritems():
      if apk in config['PACKAGES']:
        variant_name = apk + '_' + variant + '_VERSION_CODE'
        variant_val = version_code_val + config['MODIFIER']
        version_codes[variant_name] = str(variant_val)


  return version_codes

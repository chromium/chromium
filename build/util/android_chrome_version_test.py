# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from android_chrome_version import GenerateVersionCodes


class _VersionTest(unittest.TestCase):
  """Unittests for the android_chrome_version module.
  """

  EXAMPLE_VERSION_VALUES = {
      'MAJOR': '74',
      'MINOR': '0',
      'BUILD': '3720',
      'PATCH': '0',
  }

  def testGenerateVersionCodesAndroidChrome(self):
    """Assert it gives correct values for standard/example inputs"""
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm', is_next_build=False)

    chrome_version_code = output['CHROME_VERSION_CODE']

    self.assertEqual(chrome_version_code, '372000000')

  def testGenerateVersionCodesAndroidChromeModern(self):
    """Assert it gives correct values for standard/example inputs"""
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm', is_next_build=False)

    chrome_modern_version_code = output['CHROME_MODERN_VERSION_CODE']

    self.assertEqual(chrome_modern_version_code, '372000001')

  def testGenerateVersionCodesAndroidMonochrome(self):
    """Assert it gives correct values for standard/example inputs"""
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm', is_next_build=False)

    monochrome_version_code = output['MONOCHROME_VERSION_CODE']

    self.assertEqual(monochrome_version_code, '372000002')

  def testGenerateVersionCodesAndroidTrichrome(self):
    """Assert it gives correct values for standard/example inputs"""
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm', is_next_build=False)

    trichrome_version_code = output['TRICHROME_VERSION_CODE']

    self.assertEqual(trichrome_version_code, '372000003')

  def testGenerateVersionCodesAndroidNoTouch(self):
    """Assert it gives correct values for standard/example inputs"""
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm', is_next_build=False)

    notouch_chrome_version_code = output['NOTOUCH_CHROME_VERSION_CODE']

    self.assertEqual(notouch_chrome_version_code, '372000004')

  def testGenerateVersionCodesAndroidWebviewStable(self):
    """Assert it gives correct values for standard/example inputs"""
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm', is_next_build=False)

    webview_stable_version_code = output['WEBVIEW_STABLE_VERSION_CODE']

    self.assertEqual(webview_stable_version_code, '372000000')

  def testGenerateVersionCodesAndroidWebviewBeta(self):
    """Assert it gives correct values for standard/example inputs"""
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm', is_next_build=False)

    webview_beta_version_code = output['WEBVIEW_BETA_VERSION_CODE']

    self.assertEqual(webview_beta_version_code, '372000001')

  def testGenerateVersionCodesAndroidWebviewDev(self):
    """Assert it gives correct values for standard/example inputs"""
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm', is_next_build=False)

    webview_dev_version_code = output['WEBVIEW_DEV_VERSION_CODE']

    self.assertEqual(webview_dev_version_code, '372000002')

  def testGenerateVersionCodesAndroidNextBuild(self):
    """Assert it handles "next" builds correctly"""
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm', is_next_build=True)

    # Get just a sample of values
    chrome_version_code = output['CHROME_VERSION_CODE']
    monochrome_version_code = output['MONOCHROME_VERSION_CODE']
    webview_stable_version_code = output['WEBVIEW_STABLE_VERSION_CODE']
    webview_beta_version_code = output['WEBVIEW_BETA_VERSION_CODE']

    self.assertEqual(chrome_version_code, '372000005')
    self.assertEqual(monochrome_version_code, '372000007')
    self.assertEqual(webview_stable_version_code, '372000005')
    self.assertEqual(webview_beta_version_code, '372000006')

  def testGenerateVersionCodesAndroidArchArm(self):
    """Assert it handles different architectures correctly.

    Version codes for different builds need to be distinct and maintain a
    certain ordering.
    See docstring on android_chrome_version.ARCH_VERSION_CODE_DIFF for
    reasoning.
    """
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm', is_next_build=False)
    arch_chrome_version_code = output['CHROME_VERSION_CODE']

    self.assertEqual(arch_chrome_version_code, '372000000')

  def testGenerateVersionCodesAndroidArchX86(self):
    """Assert it handles different architectures correctly.

    Version codes for different builds need to be distinct and maintain a
    certain ordering.
    See docstring on android_chrome_version.ARCH_VERSION_CODE_DIFF for
    reasoning.
    """
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='x86', is_next_build=False)
    arch_chrome_version_code = output['CHROME_VERSION_CODE']

    self.assertEqual(arch_chrome_version_code, '372000010')

  def testGenerateVersionCodesAndroidArchMips(self):
    """Assert it handles different architectures correctly.

    Version codes for different builds need to be distinct and maintain a
    certain ordering.
    See docstring on android_chrome_version.ARCH_VERSION_CODE_DIFF for
    reasoning.
    """
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='mipsel', is_next_build=False)
    arch_chrome_version_code = output['CHROME_VERSION_CODE']

    self.assertEqual(arch_chrome_version_code, '372000020')

  def testGenerateVersionCodesAndroidArchArm64(self):
    """Assert it handles different architectures correctly.

    Version codes for different builds need to be distinct and maintain a
    certain ordering.
    See docstring on android_chrome_version.ARCH_VERSION_CODE_DIFF for
    reasoning.
    """
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm64', is_next_build=False)
    arch_chrome_version_code = output['CHROME_VERSION_CODE']

    self.assertEqual(arch_chrome_version_code, '372000030')

  def testGenerateVersionCodesAndroidArchArm64Variants(self):
    """Assert it handles 64-bit-specific additional version codes correctly.

    Some additional version codes are generated for 64-bit architectures.
    See docstring on android_chrome_version.ARCH64_APK_VARIANTS for more info.
    """
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm64', is_next_build=False)
    arch_monochrome_64_32_version_code = output['MONOCHROME_64_32_VERSION_CODE']
    arch_monochrome_64_version_code = output['MONOCHROME_64_VERSION_CODE']
    arch_trichrome_64_32_version_code = output['TRICHROME_64_32_VERSION_CODE']
    arch_trichrome_64_version_code = output['TRICHROME_64_VERSION_CODE']

    self.assertEqual(arch_monochrome_64_32_version_code, '372000042')
    self.assertEqual(arch_monochrome_64_version_code, '372000052')
    self.assertEqual(arch_trichrome_64_32_version_code, '372000043')
    self.assertEqual(arch_trichrome_64_version_code, '372000053')

  def testGenerateVersionCodesAndroidArchX64(self):
    """Assert it handles different architectures correctly.

    Version codes for different builds need to be distinct and maintain a
    certain ordering.
    See docstring on android_chrome_version.ARCH_VERSION_CODE_DIFF for
    reasoning.
    """
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='x64', is_next_build=False)
    arch_chrome_version_code = output['CHROME_VERSION_CODE']

    self.assertEqual(arch_chrome_version_code, '372000060')

  def testGenerateVersionCodesAndroidArchX64Variants(self):
    """Assert it handles 64-bit-specific additional version codes correctly.

    Some additional version codes are generated for 64-bit architectures.
    See docstring on android_chrome_version.ARCH64_APK_VARIANTS for more info.
    """
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='x64', is_next_build=False)
    arch_monochrome_64_32_version_code = output['MONOCHROME_64_32_VERSION_CODE']
    arch_monochrome_64_version_code = output['MONOCHROME_64_VERSION_CODE']
    arch_trichrome_64_32_version_code = output['TRICHROME_64_32_VERSION_CODE']
    arch_trichrome_64_version_code = output['TRICHROME_64_VERSION_CODE']

    self.assertEqual(arch_monochrome_64_32_version_code, '372000072')
    self.assertEqual(arch_monochrome_64_version_code, '372000082')
    self.assertEqual(arch_trichrome_64_32_version_code, '372000073')
    self.assertEqual(arch_trichrome_64_version_code, '372000083')

  def testGenerateVersionCodesAndroidArchOrderArm(self):
    """Assert it handles different architectures correctly.

    Version codes for different builds need to be distinct and maintain a
    certain ordering.
    See docstring on android_chrome_version.ARCH_VERSION_CODE_DIFF for
    reasoning.

    Test arm-related values.
    """
    arm_output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm', is_next_build=False)
    arm64_output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm64', is_next_build=False)

    arm_chrome_version_code = arm_output['CHROME_VERSION_CODE']
    arm64_chrome_version_code = arm64_output['CHROME_VERSION_CODE']

    self.assertLess(arm_chrome_version_code, arm64_chrome_version_code)

  def testGenerateVersionCodesAndroidArchOrderX86(self):
    """Assert it handles different architectures correctly.

    Version codes for different builds need to be distinct and maintain a
    certain ordering.
    See docstring on android_chrome_version.ARCH_VERSION_CODE_DIFF for
    reasoning.

    Test x86-related values.
    """
    x86_output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='x86', is_next_build=False)
    x64_output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='x64', is_next_build=False)

    x86_chrome_version_code = x86_output['CHROME_VERSION_CODE']
    x64_chrome_version_code = x64_output['CHROME_VERSION_CODE']

    self.assertLess(x86_chrome_version_code, x64_chrome_version_code)

  def testGenerateVersionCodesAndroidWebviewChannelOrderBeta(self):
    """Assert webview beta channel is higher than stable.

    The channel-specific version codes for standalone webview needs to follow
    the order stable < beta < dev.

    This allows that if a user opts into beta track, they will always have the
    beta apk, including any finch experiments targeted at beta users, even when
    beta and stable channels are otherwise on the same version.
    """
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm', is_next_build=False)

    webview_stable_version_code = output['WEBVIEW_STABLE_VERSION_CODE']
    webview_beta_version_code = output['WEBVIEW_BETA_VERSION_CODE']

    self.assertGreater(webview_beta_version_code, webview_stable_version_code)

  def testGenerateVersionCodesAndroidWebviewChannelOrderDev(self):
    """Assert webview dev channel is higher than beta.

    The channel-specific version codes for standalone webview needs to follow
    the order stable < beta < dev.

    This allows that if a user opts into dev track, they will always have the
    dev apk, including any finch experiments targeted at dev users, even when
    dev and beta channels are otherwise on the same version.
    """
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm', is_next_build=False)

    webview_beta_version_code = output['WEBVIEW_BETA_VERSION_CODE']
    webview_dev_version_code = output['WEBVIEW_DEV_VERSION_CODE']

    self.assertGreater(webview_dev_version_code, webview_beta_version_code)


if __name__ == '__main__':
  unittest.main()

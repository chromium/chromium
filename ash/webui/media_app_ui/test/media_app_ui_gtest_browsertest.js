// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview js2gtest wrapper for the chrome://media-app test suite.
 */
GEN('#include "ash/webui/media_app_ui/test/media_app_ui_browsertest.h"');

GEN('#include "ash/public/cpp/style/dark_light_mode_controller.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "third_party/blink/public/common/features.h"');

// js2gtest fixtures require var here (https://crbug.com/1033337).
// eslint-disable-next-line no-var
var MediaAppUIGtestBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return 'chrome://media-app';
  }

  /** @override */
  get isAsync() {
    return true;
  }

  /** @override */
  get featureList() {
    // The FileHandling flag should be automatically set by origin trials, but
    // this testing environment does not seem to recognize origin trials. To
    // work around it they must be explicitly set with flags to prevent tests
    // crashing on Media App load due to window.launchQueue being undefined.
    // See http://crbug.com/1071320.
    return {enabled: ['blink::features::kFileHandlingAPI']};
  }

  /** @override */
  get typedefCppFixture() {
    return 'MediaAppUiBrowserTest';
  }
};

// js2gtest fixtures require var here (https://crbug.com/1033337).
// eslint-disable-next-line no-var
var MediaAppUIWithLightModeGtestBrowserTest =
    class extends MediaAppUIGtestBrowserTest {
  /** @override */
  get testGenPreamble() {
    return () => {
      // Switch to light mode.
      GEN('ash::DarkLightModeController::Get()->SetDarkModeEnabledForTest(false);');
    };
  }
};

// js2gtest fixtures require var here (https://crbug.com/1033337).
// eslint-disable-next-line no-var
var MediaAppUIWithDarkModeGtestBrowserTest =
    class extends MediaAppUIGtestBrowserTest {
  /** @override */
  get testGenPreamble() {
    return () => {
      // Switch to dark mode.
      GEN('ash::DarkLightModeController::Get()->SetDarkModeEnabledForTest(true);');
    };
  }
};

async function GetTestHarness() {
  const testHarnessPolicy = trustedTypes.createPolicy('test-harness', {
    createScriptURL: () => './media_app_ui_browsertest.js',
  });

  const tests =
      /** @type {!HTMLScriptElement} */ (document.createElement('script'));
  tests.src = testHarnessPolicy.createScriptURL('');
  tests.type = 'module';
  await new Promise((resolve, reject) => {
    tests.onload = resolve;
    tests.onerror = reject;
    document.body.appendChild(tests);
  });

  // When media_app_ui_browsertest.js loads, it will add this onto the window.
  return window['MediaAppUIBrowserTest_for_js2gtest'];
}

async function runMediaAppTest(name, guest = false) {
  const MediaAppUIBrowserTest = await GetTestHarness();
  try {
    if (guest) {
      await MediaAppUIBrowserTest.runTestInGuest(name);
    } else {
      await MediaAppUIBrowserTest[name]();
    }
    testDone();
  } catch (/* @type {Error} */ error) {
    let message = 'exception';
    if (typeof error === 'object' && error !== null && error['message']) {
      message = error['message'];
      console.log(error['stack']);
    } else {
      console.log(error);
    }
    /** @type {function(*)} */ (testDone)([false, message]);
  }
}

function runTestInGuest(name) {
  return runMediaAppTest(name, true);
}

// Ensure every test body has a TEST_F call in this file.
TEST_F('MediaAppUIGtestBrowserTest', 'ConsistencyCheck', async () => {
  const MediaAppUIBrowserTest = await GetTestHarness();
  const bodies = {
    ...(/** @type {{testCaseBodies: Object}} */ (MediaAppUIGtestBrowserTest))
        .testCaseBodies,
    ...(/** @type {{testCaseBodies: Object}} */ (
            MediaAppUIWithLightModeGtestBrowserTest))
        .testCaseBodies,
    ...(/** @type {{testCaseBodies: Object}} */ (
            MediaAppUIWithDarkModeGtestBrowserTest))
        .testCaseBodies,
  };
  for (const f in MediaAppUIBrowserTest) {
    if (f === 'runTestInGuest') {
      continue;
    }
    if (!(f in bodies || `DISABLED_${f}` in bodies)) {
      console.error(`MediaAppUIBrowserTest.${f} missing a TEST_F`);
    }
  }
  testDone();
});

TEST_F('MediaAppUIGtestBrowserTest', 'GuestCanLoad', () => {
  runMediaAppTest('GuestCanLoad');
});

TEST_F('MediaAppUIGtestBrowserTest', 'HasTitleAndLang', () => {
  runMediaAppTest('HasTitleAndLang');
});

TEST_F('MediaAppUIGtestBrowserTest', 'LaunchFile', () => {
  runMediaAppTest('LaunchFile');
});

TEST_F('MediaAppUIGtestBrowserTest', 'ReportsErrorsFromTrustedContext', () => {
  runMediaAppTest('ReportsErrorsFromTrustedContext');
});

TEST_F('MediaAppUIGtestBrowserTest', 'NonLaunchableIpcAfterFastLoad', () => {
  runMediaAppTest('NonLaunchableIpcAfterFastLoad');
});

TEST_F('MediaAppUIGtestBrowserTest', 'ReLaunchableAfterFastLoad', () => {
  runMediaAppTest('ReLaunchableAfterFastLoad');
});

TEST_F('MediaAppUIGtestBrowserTest', 'MultipleFilesHaveTokens', () => {
  runMediaAppTest('MultipleFilesHaveTokens');
});

TEST_F('MediaAppUIGtestBrowserTest', 'SingleAudioLaunch', () => {
  runMediaAppTest('SingleAudioLaunch');
});

TEST_F('MediaAppUIGtestBrowserTest', 'MultipleSelectionLaunch', () => {
  runMediaAppTest('MultipleSelectionLaunch');
});

TEST_F(
    'MediaAppUIWithLightModeGtestBrowserTest', 'NotifyCurrentFileLight', () => {
      runMediaAppTest('NotifyCurrentFileLight');
    });

TEST_F(
    'MediaAppUIWithDarkModeGtestBrowserTest', 'NotifyCurrentFileDark', () => {
      runMediaAppTest('NotifyCurrentFileDark');
    });

TEST_F(
    'MediaAppUIWithDarkModeGtestBrowserTest', 'NotifyCurrentFileAppIconDark',
    () => {
      runMediaAppTest('NotifyCurrentFileAppIconDark');
    });

TEST_F('MediaAppUIGtestBrowserTest', 'LaunchUnopenableFile', () => {
  runMediaAppTest('LaunchUnopenableFile');
});

TEST_F('MediaAppUIGtestBrowserTest', 'LaunchUnnavigableDirectory', () => {
  runMediaAppTest('LaunchUnnavigableDirectory');
});

TEST_F('MediaAppUIGtestBrowserTest', 'NavigateWithUnopenableSibling', () => {
  runMediaAppTest('NavigateWithUnopenableSibling');
});

TEST_F('MediaAppUIGtestBrowserTest', 'FileThatBecomesDirectory', () => {
  runMediaAppTest('FileThatBecomesDirectory');
});

TEST_F('MediaAppUIGtestBrowserTest', 'CanOpenFeedbackDialog', () => {
  runMediaAppTest('CanOpenFeedbackDialog');
});

TEST_F('MediaAppUIGtestBrowserTest', 'CanFullscreenVideo', () => {
  runMediaAppTest('CanFullscreenVideo');
});

TEST_F('MediaAppUIGtestBrowserTest', 'LoadVideoWithSubtitles', () => {
  runMediaAppTest('LoadVideoWithSubtitles');
});

TEST_F('MediaAppUIGtestBrowserTest', 'OverwriteOriginalIPC', () => {
  runMediaAppTest('OverwriteOriginalIPC');
});

TEST_F('MediaAppUIGtestBrowserTest', 'RejectZeroByteWrites', () => {
  runMediaAppTest('RejectZeroByteWrites');
});

TEST_F('MediaAppUIGtestBrowserTest', 'OverwriteOriginalPickerFallback', () => {
  runMediaAppTest('OverwriteOriginalPickerFallback');
});

TEST_F('MediaAppUIGtestBrowserTest', 'FilePickerValidateExtension', () => {
  runMediaAppTest('FilePickerValidateExtension');
});

TEST_F('MediaAppUIGtestBrowserTest', 'CrossContextErrors', () => {
  runMediaAppTest('CrossContextErrors');
});

TEST_F('MediaAppUIGtestBrowserTest', 'DeleteOriginalIPC', () => {
  runMediaAppTest('DeleteOriginalIPC');
});

TEST_F('MediaAppUIGtestBrowserTest', 'DeletionOpensNextFile', () => {
  runMediaAppTest('DeletionOpensNextFile');
});

TEST_F('MediaAppUIGtestBrowserTest', 'DeleteMissingFile', () => {
  runMediaAppTest('DeleteMissingFile');
});

TEST_F('MediaAppUIGtestBrowserTest', 'RenameMissingFile', () => {
  runMediaAppTest('RenameMissingFile');
});

TEST_F('MediaAppUIGtestBrowserTest', 'OpenAllowedFileIPC', () => {
  runMediaAppTest('OpenAllowedFileIPC');
});

TEST_F('MediaAppUIGtestBrowserTest', 'NavigateIPC', () => {
  runMediaAppTest('NavigateIPC');
});

TEST_F('MediaAppUIGtestBrowserTest', 'NavigateOutOfSync', () => {
  runMediaAppTest('NavigateOutOfSync');
});

TEST_F('MediaAppUIGtestBrowserTest', 'RenameOriginalIPC', () => {
  runMediaAppTest('RenameOriginalIPC');
});

TEST_F('MediaAppUIGtestBrowserTest', 'RequestSaveFileIPC', () => {
  runMediaAppTest('RequestSaveFileIPC');
});

TEST_F('MediaAppUIGtestBrowserTest', 'GetExportFileIPC', () => {
  runMediaAppTest('GetExportFileIPC');
});

TEST_F('MediaAppUIGtestBrowserTest', 'SaveAsIPC', () => {
  runMediaAppTest('SaveAsIPC');
});

TEST_F('MediaAppUIGtestBrowserTest', 'SaveAsErrorHandling', () => {
  runMediaAppTest('SaveAsErrorHandling');
});

TEST_F('MediaAppUIGtestBrowserTest', 'OpenFilesWithFilePickerIPC', () => {
  runMediaAppTest('OpenFilesWithFilePickerIPC');
});

TEST_F('MediaAppUIGtestBrowserTest', 'RelatedFiles', () => {
  runMediaAppTest('RelatedFiles');
});

TEST_F('MediaAppUIGtestBrowserTest', 'SortedFilesByTime', () => {
  runMediaAppTest('SortedFilesByTime');
});

TEST_F('MediaAppUIGtestBrowserTest', 'SortedFilesByName', () => {
  runMediaAppTest('SortedFilesByName');
});

TEST_F('MediaAppUIGtestBrowserTest', 'GetFileNotCalledOnAllFiles', () => {
  runMediaAppTest('GetFileNotCalledOnAllFiles');
});

TEST_F('MediaAppUIGtestBrowserTest', 'GuestHasFocus', () => {
  runMediaAppTest('GuestHasFocus');
});

TEST_F(
    'MediaAppUIWithLightModeGtestBrowserTest',
    'BodyHasCorrectBackgroundColorInLightMode', () => {
      runMediaAppTest('BodyHasCorrectBackgroundColorInLightMode');
    });

// Test cases injected into the guest context.
// See implementations in media_app_guest_ui_browsertest.js.

TEST_F('MediaAppUIGtestBrowserTest', 'GuestCanSpawnWorkers', () => {
  runTestInGuest('GuestCanSpawnWorkers');
});

TEST_F('MediaAppUIGtestBrowserTest', 'GuestHasLang', () => {
  runTestInGuest('GuestHasLang');
});

TEST_F('MediaAppUIGtestBrowserTest', 'GuestLoadsLoadTimeData', () => {
  runTestInGuest('GuestLoadsLoadTimeData');
});

TEST_F('MediaAppUIGtestBrowserTest', 'GuestCanLoadWithCspRestrictions', () => {
  runTestInGuest('GuestCanLoadWithCspRestrictions');
});

TEST_F('MediaAppUIGtestBrowserTest', 'GuestStartsWithDefaultFileList', () => {
  runTestInGuest('GuestStartsWithDefaultFileList');
});

TEST_F('MediaAppUIGtestBrowserTest', 'GuestFailsToFetchMissingFonts', () => {
  runTestInGuest('GuestFailsToFetchMissingFonts');
});

TEST_F('MediaAppUIGtestBrowserTest', 'GuestCanFilterInPlace', () => {
  runTestInGuest('GuestCanFilterInPlace');
});

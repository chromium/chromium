// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../common/testing/e2e_test_base.js']);

/** FazeGaze MediaPipe tests. */
FaceGazeMediaPipeTest = class extends E2ETestBase {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    await importModule(
        ['FaceLandmarker', 'FilesetResolver'],
        '/accessibility_common/facegaze/mediapipe_task_vision/task_vision.js');
  }

  /** @override */
  testGenCppIncludes() {
    super.testGenCppIncludes();
    GEN(`
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "ui/accessibility/accessibility_features.h"
    `);
  }

  /** @override */
  testGenPreamble() {
    super.testGenPreamble();
    GEN(`base::OnceClosure load_cb =
        base::BindOnce(&ash::AccessibilityManager::EnableFaceGaze,
            base::Unretained(ash::AccessibilityManager::Get()), true);`);
    // TODO(b/309121742): change `failOnConsoleError` to true and specify
    // allowed messages from mediapipe wasm.
    super.testGenPreambleCommon(
        /*extensionIdName=*/ 'kAccessibilityCommonExtensionId',
        /*failOnConsoleError=*/ false);
  }

  /** @override */
  get featureList() {
    return {enabled: ['features::kAccessibilityFaceGaze']};
  }
};

AX_TEST_F('FaceGazeMediaPipeTest', 'SmokeTest', async function() {
  assertTrue(Boolean(FilesetResolver));
  assertTrue(Boolean(FaceLandmarker));

  const resolver = await FilesetResolver.forVisionTasks(
      '/accessibility_common/facegaze/mediapipe_task_vision');
  assertTrue(Boolean(resolver));
  const faceLandmarker = await FaceLandmarker.createFromOptions(resolver, {
    baseOptions: {
      modelAssetPath: '/accessibility_common/facegaze/mediapipe_task_vision/' +
          'face_landmarker.task',
    },
    runningMode: 'VIDEO',
  });
  assertTrue(Boolean(faceLandmarker));
});

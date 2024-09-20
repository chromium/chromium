// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_MV2_EXPERIMENT_STAGE_H_
#define CHROME_BROWSER_EXTENSIONS_MV2_EXPERIMENT_STAGE_H_

namespace extensions {

// The current stage of the Manifest V2 deprecation. Note that for all stages,
// these only refer to affected extensions.
enum class MV2ExperimentStage {
  // The user is not in any stage; that is, Manifest V2 extensions are fully
  // supported.
  kNone,
  // The user is warned that Manifest V2 extensions are deprecated and will
  // soon be unsupported.
  kWarning,
  // Unsupported Manifest V2 extensions will be disabled. The user is allowed
  // to re-enable them at any point.
  kDisableWithReEnable,
  // Manifest V2 extensions are no longer supported. The user is not able to
  // re-enable them.
  kUnsupported,

  // TODO(https://crbug.com/337191307): Continue adding more experiment stages
  // here. For each new entry, update Mv2ExperimentStage in
  // chrome/browser/resources/extensions/mv2_deprecation_util.ts.
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_MV2_EXPERIMENT_STAGE_H_

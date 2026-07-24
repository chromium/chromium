// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {LhsChipIdentifier} from './toolbar_ui_api_data_model.mojom-webui.js';

export interface PermissionChipDelegate {
  onChipClicked(identifier: LhsChipIdentifier, isPointer: boolean): void;
  onChipPointerEntered(identifier: LhsChipIdentifier): void;
  onChipPointerExited(identifier: LhsChipIdentifier): void;
  onChipMousePressed(identifier: LhsChipIdentifier): void;
  onChipExpandAnimationEnded(identifier: LhsChipIdentifier): void;
  onChipCollapseAnimationEnded(identifier: LhsChipIdentifier): void;
}

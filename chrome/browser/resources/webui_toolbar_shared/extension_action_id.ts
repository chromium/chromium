// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ExtensionActionId} from './extensions_bar_data_model.mojom-webui.js';

// Key identifying the request access button. Also used in the secondary
// identifier of the button's TrackedElement, so must match
// kExtensionsRequestAccessButtonActionId in
// webui_toolbar_extensions_container.h. Not a valid extension ID, so can't
// collide with the key of an extension action.
const REQUEST_ACCESS_BUTTON_KEY = 'ExtensionsRequestAccessButton';

// Returns a string uniquely identifying the button with `id`, suitable for use
// as a key: the extension ID for extension actions, an empty string for the
// extensions menu button, or REQUEST_ACCESS_BUTTON_KEY for the request access
// button. Matches WebUIToolbarExtensionsContainer::GetActionKey().
export function getExtensionActionKey(id: ExtensionActionId): string {
  if (isExtensionAction(id)) {
    return id.extensionId!;
  }
  if (isRequestAccessButton(id)) {
    return REQUEST_ACCESS_BUTTON_KEY;
  }
  return '';
}

// Returns whether `id` is for an actual extension, as opposed to the
// extensions menu button or the request access button.
export function isExtensionAction(id: ExtensionActionId): boolean {
  return id.extensionId !== undefined && id.extensionId !== null;
}

// Returns whether `id` is for the request access button.
export function isRequestAccessButton(id: ExtensionActionId): boolean {
  return id.requestAccessButton !== undefined &&
      id.requestAccessButton !== null;
}

// Returns whether `id` is for the extensions menu button (puzzle piece).
export function isExtensionsMenuButton(id: ExtensionActionId): boolean {
  return id.extensionsMenuButton !== undefined &&
      id.extensionsMenuButton !== null;
}

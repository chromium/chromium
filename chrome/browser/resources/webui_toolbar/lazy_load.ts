// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Holds non-navigation toolbar controls and test exports that
 * are excluded from the initial webui_toolbar.js bundle so InitialWebUI
 * startup only parses and defines <toolbar-app> and the core navigation
 * buttons.
 */

// Register every toolbar iconset before any control is defined. Icon names
// come from icon_table.cc and can be handed to any <icon-from-table>, so a
// consuming element cannot know in advance which iconset it needs.
import './icons.js';
// <if expr="_google_chrome">
import './internal/icons.js';
// </if>
import '/shared/icon_from_table.js';
import './app_menu_button.js';
import './avatar_button.js';
import './battery_saver_button.js';
import './extensions.js';
import './glic_button.js';
import './location_bar.js';
import './media_button.js';
import './overflow_button.js';
import './performance_intervention_button.js';
import './pinned_toolbar_actions.js';

export {AnimationTracker} from '/shared/animation_tracker.js';
export {FlatEdge} from '/shared/extensions_bar_data_model.mojom-webui.js';
export type {ExtensionActionId, ExtensionActionInfo} from '/shared/extensions_bar_data_model.mojom-webui.js';
export type {IconFromTableElement} from '/shared/icon_from_table.js';
export {PermissionChipElement} from '/shared/permission_chip.js';
export type {PermissionDashboardElement} from '/shared/permission_dashboard.js';
export {AppMenuButtonElement} from './app_menu_button.js';
export type {AvatarButtonElement} from './avatar_button.js';
export {BatterySaverButtonElement} from './battery_saver_button.js';
export {ContentSettingIconElement} from './content_setting_icon.js';
export type {ExtensionsElement} from './extensions.js';
export {GlicButtonElement} from './glic_button.js';
export {LocationBarElement} from './location_bar.js';
export {LocationIconElement} from './location_icon.js';
export {MediaButtonElement} from './media_button.js';
export {OverflowableToolbarActionContainerMixin} from './overflowable_toolbar_action_container_mixin.js';
export {OverflowableToolbarActionMixin} from './overflowable_toolbar_action_mixin.js';
export type {OverflowableToolbarActionElement} from './overflowable_toolbar_action_mixin.js';
export {PageActionIconElement} from './page_action_icon.js';
export type {PinnedToolbarActionElement} from './pinned_toolbar_action.js';
export type {PinnedToolbarActionsElement} from './pinned_toolbar_actions.js';
export {PointerProxyImpl} from './pointer_proxy.js';
export type {PointerProxy} from './pointer_proxy.js';
export {ReadonlyOmniboxElement} from './readonly_omnibox.js';
export {ToolbarActionContainerMixin} from './toolbar_action_container_mixin.js';
export type {KeyedActionState, ToolbarActionContainerMixinInterface} from './toolbar_action_container_mixin.js';
export {ToolbarActionMixin} from './toolbar_action_mixin.js';
export type {ToolbarActionMixinInterface} from './toolbar_action_mixin.js';
export {ToolbarChipButtonElement} from './toolbar_chip_button.js';

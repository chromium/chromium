// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TrackedElementManager} from '//resources/js/tracked_element/tracked_element_manager.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {getCss} from './permission_chip.css.js';
import {getHtml} from './permission_chip.html.js';
import {LhsChipIdentifier, PermissionAction, PermissionChipTheme, PermissionPromptStyle} from './toolbar_ui_api_data_model.mojom-webui.js';
import type {PermissionChipState} from './toolbar_ui_api_data_model.mojom-webui.js';

export class PermissionChipElement extends CrLitElement {
  static get is() {
    return 'permission-chip';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      chipState: {type: Object},
    };
  }

  accessor chipState: PermissionChipState|null = null;

  private trackedElementManager_: TrackedElementManager;

  constructor() {
    super();
    this.trackedElementManager_ = TrackedElementManager.getInstance();
  }

  override connectedCallback() {
    super.connectedCallback();
    const id = this.id === 'request-chip' ?
        'PermissionChipView::kPermissionRequestChipElementId' :
        'PermissionChipView::kIndicatorChipElementId';
    this.trackedElementManager_.startTracking(this, id);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.trackedElementManager_.stopTracking(this);
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('chipState')) {
      this.updateColors_();
    }
  }

  private getIdentifier_(): LhsChipIdentifier {
    return this.id === 'request-chip' ? LhsChipIdentifier.kPermissionRequest :
                                        LhsChipIdentifier.kPermissionIndicator;
  }

  protected onPointerenter_() {
    BrowserProxyImpl.getInstance().toolbarUIHandler.onLhsChipPointerEntered(
        this.getIdentifier_());
  }

  protected onPointerleave_() {
    BrowserProxyImpl.getInstance().toolbarUIHandler.onLhsChipPointerExited(
        this.getIdentifier_());
  }

  protected onPointercancel_() {
    this.onPointerleave_();
  }

  // Computes the foreground and background colors for the chip based on its
  // logical state (theme, user decision, prompt style).
  // Replicating this logic in TypeScript instead of passing explicit colors
  // over Mojo is the standard WebUI approach. It preserves the separation of
  // concerns: C++ dictates the logical state, and WebUI CSS handles the actual
  // color resolution (automatically managing light/dark mode variations).
  // This logic must strictly mirror the Native Views implementation found in:
  // - PermissionChipView::GetForegroundColor()
  // - PermissionChipView::GetBackgroundColor()
  private updateColors_() {
    if (!this.chipState) {
      return;
    }

    let bgColor = 'var(--color-omnibox-chip-background)';
    let fgColor = 'var(--color-omnibox-chip-foreground-normal-visibility)';

    switch (this.chipState.theme) {
      case PermissionChipTheme.kActivityIndicator:
      case PermissionChipTheme.kInUseActivityIndicator:
        bgColor =
            'var(--color-omnibox-chip-in-use-activity-indicator-background)';
        fgColor =
            'var(--color-omnibox-chip-in-use-activity-indicator-foreground)';
        break;
      case PermissionChipTheme.kBlockedActivityIndicator:
        bgColor =
            'var(--color-omnibox-chip-blocked-activity-indicator-background)';
        fgColor =
            'var(--color-omnibox-chip-blocked-activity-indicator-foreground)';
        break;
      case PermissionChipTheme.kOnSystemBlockedActivityIndicator:
        bgColor = 'var(--color-omnibox-chip-' +
            'on-system-blocked-activity-indicator-background)';
        fgColor = 'var(--color-omnibox-chip-' +
            'on-system-blocked-activity-indicator-foreground)';
        break;
      case PermissionChipTheme.kLowVisibility:
        fgColor = 'var(--color-omnibox-chip-foreground-low-visibility)';
        break;
      case PermissionChipTheme.kNormalVisibility:
      default:
        break;
    }

    // Apply native fallback color logic for foreground text and icon.
    // This logic only applies if the theme isn't one of the special indicators.
    // (In native, GetForegroundColor() returns early for the indicators above).
    if (this.chipState.theme === PermissionChipTheme.kNormalVisibility ||
        this.chipState.theme === PermissionChipTheme.kLowVisibility) {
      if (this.chipState.promptStyle === PermissionPromptStyle.kQuietChip) {
        fgColor = 'var(--color-omnibox-chip-foreground-low-visibility)';
      }

      switch (this.chipState.userDecision) {
        case PermissionAction.kGranted:
        case PermissionAction.kGrantedOnce:
          fgColor = 'var(--color-omnibox-chip-foreground-normal-visibility)';
          break;
        case PermissionAction.kDenied:
        case PermissionAction.kDismissed:
        case PermissionAction.kIgnored:
        case PermissionAction.kRevoked:
          fgColor = 'var(--color-omnibox-chip-foreground-low-visibility)';
          break;
        default:
          break;
      }

      if (this.chipState.shouldShowBlockedIcon) {
        fgColor = 'var(--color-omnibox-chip-foreground-low-visibility)';
      }
    }

    this.style.setProperty('--chip-bg-color', bgColor);
    this.style.setProperty('--chip-fg-color', fgColor);
  }

  protected getIconUrl_(): string {
    if (!this.chipState || !this.chipState.iconName) {
      return '';
    }

    let iconName = '';
    switch (this.chipState.iconName) {
      case 'kLocationOnChromeRefreshIcon':
        iconName = 'location_on_chrome_refresh';
        break;
      case 'kLocationOffChromeRefreshIcon':
        iconName = 'location_off_chrome_refresh';
        break;
      case 'kVideocamChromeRefreshIcon':
        iconName = 'videocam_chrome_refresh';
        break;
      case 'kVideocamOffChromeRefreshIcon':
        iconName = 'videocam_off_chrome_refresh';
        break;
      case 'kMicChromeRefreshIcon':
        iconName = 'mic_chrome_refresh';
        break;
      case 'kMicOffChromeRefreshIcon':
        iconName = 'mic_off_chrome_refresh';
        break;
      case 'kNotificationsChromeRefreshIcon':
        iconName = 'notifications_chrome_refresh';
        break;
      case 'kNotificationsOffChromeRefreshIcon':
        iconName = 'notifications_off_chrome_refresh';
        break;
      default:
        break;
    }
    return iconName ? `url('rhs_icons/${iconName}.svg')` : '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'permission-chip': PermissionChipElement;
  }
}

customElements.define(PermissionChipElement.is, PermissionChipElement);

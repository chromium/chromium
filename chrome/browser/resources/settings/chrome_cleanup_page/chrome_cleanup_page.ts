// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import '../controls/controlled_button.js';
import '../controls/settings_checkbox.js';
import '../prefs/prefs.js';
import '../settings_shared.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsCheckboxElement} from '../controls/settings_checkbox.js';

import {getTemplate} from './chrome_cleanup_page.html.js';
import {ChromeCleanupProxy, ChromeCleanupProxyImpl} from './chrome_cleanup_proxy.js';
import {ChromeCleanupRemovalListItem} from './items_to_remove_list.js';

/**
 * The reason why the controller is in state kIdle.
 * Must be kept in sync with ChromeCleanerController::IdleReason.
 */
export enum ChromeCleanupIdleReason {
  INITIAL = 'initial',
  REPORTER_FOUND_NOTHING = 'reporter_found_nothing',
  REPORTER_FAILED = 'reporter_failed',
  SCANNING_FOUND_NOTHING = 'scanning_found_nothing',
  SCANNING_FAILED = 'scanning_failed',
  CONNECTION_LOST = 'connection_lost',
  USER_DECLINED_CLEANUP = 'user_declined_cleanup',
  CLEANING_FAILED = 'cleaning_failed',
  CLEANING_SUCCEEDED = 'cleaning_succeeded',
  CLEANER_DOWNLOAD_FAILED = 'cleaner_download_failed',
}

/**
 * The possible states for the cleanup card.
 */
enum ChromeCleanerCardState {
  SCANNING_OFFERED = 'scanning_offered',
  SCANNING = 'scanning',
  CLEANUP_OFFERED = 'cleanup_offered',
  CLEANING = 'cleaning',
  REBOOT_REQUIRED = 'reboot_required',
  SCANNING_FOUND_NOTHING = 'scanning_found_nothing',
  SCANNING_FAILED = 'scanning_failed',
  CLEANUP_SUCCEEDED = 'cleanup_succeeded',
  CLEANING_FAILED = 'cleanup_failed',
  CLEANER_DOWNLOAD_FAILED = 'cleaner_download_failed',
}

/**
 * Boolean properties for a cleanup card state.
 */
enum ChromeCleanupCardFlags {
  NONE = 0,
  SHOW_LOGS_PERMISSIONS = 1 << 0,
  WAITING_FOR_RESULT = 1 << 1,
  SHOW_ITEMS_TO_REMOVE = 1 << 2,
}

/**
 * Identifies an ongoing scanning/cleanup action.
 */
enum ChromeCleanupOngoingAction {
  NONE = 0,
  SCANNING = 1,
  CLEANING = 2,
}

interface ChromeCleanupCardActionButton {
  label: string;
  doAction: () => void;
}

interface ChromeCleanupCardComponents {
  title: string|null;
  explanation: string|null;
  actionButton: ChromeCleanupCardActionButton|null;
  flags: number;
}

/**
 * Represents the file path structure of a base::FilePath.
 * dirname ends with a separator.
 */
export interface ChromeCleanupFilePath {
  dirname: string;
  basename: string;
}

export interface ChromeCleanerScannerResults {
  files: ChromeCleanupFilePath[];
  registryKeys: string[];
}

/**
 * @fileoverview
 * 'settings-chrome-cleanup-page' is the settings page containing Chrome
 * Cleanup settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <settings-chrome-cleanup-page></settings-chrome-cleanup-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 */

export interface SettingsChromeCleanupPageElement {
  $: {
    chromeCleanupLogsUploadControl: SettingsCheckboxElement,
    chromeCleanupShowNotificationControl: SettingsCheckboxElement,
  };
}

const SettingsChromeCleanupPageElementBase =
    WebUiListenerMixin(I18nMixin(PolymerElement));

export class SettingsChromeCleanupPageElement extends
    SettingsChromeCleanupPageElementBase {
  static get is() {
    return 'settings-chrome-cleanup-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      title_: {
        type: String,
        value: '',
      },

      explanation_: {
        type: String,
        value: '',
      },

      isWaitingForResult_: {
        type: Boolean,
        value: '',
      },

      showActionButton_: {
        type: Boolean,
        value: false,
      },

      cleanupEnabled_: {
        type: Boolean,
        value: true,
      },

      actionButtonLabel_: {
        type: String,
        value: '',
      },

      showExplanation_: {
        type: Boolean,
        computed: 'computeShowExplanation_(explanation_)',
      },

      showLogsPermission_: {
        type: Boolean,
        value: false,
      },

      showItemsToRemove_: {
        type: Boolean,
        value: false,
      },

      itemsToRemoveSectionExpanded_: {
        type: Boolean,
        value: false,
        observer: 'itemsToRemoveSectionExpandedChanged_',
      },

      showItemsLinkLabel_: {
        type: String,
        value: '',
      },

      showingAllFiles_: {
        type: Boolean,
        value: false,
      },

      scannerResults_: {
        type: Object,
        value() {
          return {'files': [], 'registryKeys': []};
        },
      },

      hasFilesToShow_: {
        type: Boolean,
        computed: 'computeHasFilesToShow_(scannerResults_)',
      },

      hasRegistryKeysToShow_: {
        type: Boolean,
        computed: 'computeHasRegistryKeysToShow_(scannerResults_)',
      },

      logsUploadPref_: {
        type: Object,
        value() {
          return {};
        },
      },

      isPoweredByPartner_: {
        type: Boolean,
        value: false,
      },

      /**
       * Virtual pref that's attached to the notification checkbox.
       */
      notificationEnabledPref_: {
        type: Object,
        value() {
          return {
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          };
        },
      },
    };
  }

  prefs: {software_reporter: {reporting: chrome.settingsPrivate.PrefObject}};
  private title_: TrustedHTML;
  private explanation_: string;
  private isWaitingForResult_: boolean;
  private showActionButton_: boolean;
  private cleanupEnabled_: boolean;
  private actionButtonLabel_: string;
  private showExplanation_: boolean;
  private showLogsPermission_: boolean;
  private showNotificationPermission_: boolean;
  private showItemsToRemove_: boolean;
  private itemsToRemoveSectionExpanded_: boolean;
  private showItemsLinkLabel_: string;
  private showingAllFiles_: boolean;
  private scannerResults_: ChromeCleanerScannerResults;
  private hasFilesToShow_: boolean;
  private hasRegistryKeysToShow_: boolean;
  private logsUploadPref_: chrome.settingsPrivate.PrefObject;
  private isPoweredByPartner_: boolean;
  private notificationEnabledPref_: chrome.settingsPrivate.PrefObject;

  private emptyChromeCleanerScannerResults_:
      ChromeCleanerScannerResults = {files: [], registryKeys: []};
  private browserProxy_: ChromeCleanupProxy =
      ChromeCleanupProxyImpl.getInstance();
  private doAction_: (() => void)|null = null;
  private cardStateToComponentsMap_:
      Map<ChromeCleanerCardState, ChromeCleanupCardComponents>|null = null;
  private ongoingAction_: ChromeCleanupOngoingAction =
      ChromeCleanupOngoingAction.NONE;
  private renderScanOfferedByDefault_: boolean;

  constructor() {
    super();

    /**
     * If true; the scan offered view is rendered on state idle, regardless of
     * the idle reason received from the cleaner controller. The goal is to
     * ignore previous interactions (such as completed cleanups) performed on
     * other tabs or if this tab is reloaded.
     * Set to false whenever there is a transition to a non-idle state while the
     * current tab is open.
     */
    this.renderScanOfferedByDefault_ = true;
  }

  override connectedCallback() {
    super.connectedCallback();

    this.cardStateToComponentsMap_ = this.buildCardStateToComponentsMap_();

    this.addWebUiListener(
        'chrome-cleanup-on-idle',
        (idleReason: string) => this.onIdle_(idleReason));
    this.addWebUiListener(
        'chrome-cleanup-on-scanning', () => this.onScanning_());
    // Note: both reporter running and scanning share the same UI.
    this.addWebUiListener(
        'chrome-cleanup-on-reporter-running', () => this.onScanning_());
    this.addWebUiListener(
        'chrome-cleanup-on-infected',
        (isPoweredByPartner: boolean,
         scannerResults: ChromeCleanerScannerResults) =>
            this.onInfected_(isPoweredByPartner, scannerResults));
    this.addWebUiListener(
        'chrome-cleanup-on-cleaning',
        (isPoweredByPartner: boolean,
         scannerResults: ChromeCleanerScannerResults) =>
            this.onCleaning_(isPoweredByPartner, scannerResults));
    this.addWebUiListener(
        'chrome-cleanup-on-reboot-required', () => this.onRebootRequired_());
    this.addWebUiListener(
        'chrome-cleanup-enabled-change',
        (enabled: boolean) => this.onCleanupEnabledChange_(enabled));
    this.browserProxy_.registerChromeCleanerObserver();
  }

  /**
   * Implements the action for the only visible button in the UI, which can be
   * either to start an action such as a cleanup or to restart the computer.
   */
  private proceed_() {
    this.doAction_!();
  }

  /**
   * Notifies Chrome that the details section was opened or closed.
   */
  private itemsToRemoveSectionExpandedChanged_(
      newVal: boolean, oldVal: boolean) {
    if (!oldVal && newVal) {
      this.browserProxy_.notifyShowDetails(this.itemsToRemoveSectionExpanded_);
    }
  }

  private computeShowExplanation_(explanation: string): boolean {
    return explanation !== '';
  }

  /**
   * @param scannerResults The cleanup items to be presented to the user.
   * @return Whether there are files to show to the user.
   */
  private computeHasFilesToShow_(scannerResults: ChromeCleanerScannerResults):
      boolean {
    return scannerResults.files.length > 0;
  }

  /**
   * @param scannerResults The cleanup items to be presented to the user.
   * @return Whether user-initiated cleanups are enabled and there are registry
   *     keys to show to the user.
   */
  private computeHasRegistryKeysToShow_(
      scannerResults: ChromeCleanerScannerResults): boolean {
    return scannerResults.registryKeys.length > 0;
  }

  /**
   * Listener of event 'chrome-cleanup-on-idle'.
   */
  private onIdle_(idleReason: string) {
    const lastAction = this.ongoingAction_;
    this.ongoingAction_ = ChromeCleanupOngoingAction.NONE;
    this.scannerResults_ = this.emptyChromeCleanerScannerResults_;

    // Ignore the idle reason and render the scan offered view if no
    // interaction happened on this tab.
    if (this.renderScanOfferedByDefault_) {
      idleReason = ChromeCleanupIdleReason.INITIAL;
    }

    switch (idleReason) {
      case ChromeCleanupIdleReason.INITIAL:
        this.renderCleanupCard_(ChromeCleanerCardState.SCANNING_OFFERED);
        break;

      case ChromeCleanupIdleReason.SCANNING_FOUND_NOTHING:
      case ChromeCleanupIdleReason.REPORTER_FOUND_NOTHING:
        this.renderCleanupCard_(ChromeCleanerCardState.SCANNING_FOUND_NOTHING);
        break;

      case ChromeCleanupIdleReason.SCANNING_FAILED:
      case ChromeCleanupIdleReason.REPORTER_FAILED:
        this.renderCleanupCard_(ChromeCleanerCardState.SCANNING_FAILED);
        break;

      case ChromeCleanupIdleReason.CONNECTION_LOST:
        if (lastAction === ChromeCleanupOngoingAction.SCANNING) {
          this.renderCleanupCard_(ChromeCleanerCardState.SCANNING_FAILED);
        } else {
          assert(lastAction === ChromeCleanupOngoingAction.CLEANING);
          this.renderCleanupCard_(ChromeCleanerCardState.CLEANING_FAILED);
        }
        break;

      case ChromeCleanupIdleReason.CLEANING_FAILED:
      case ChromeCleanupIdleReason.USER_DECLINED_CLEANUP:
        this.renderCleanupCard_(ChromeCleanerCardState.CLEANING_FAILED);
        break;

      case ChromeCleanupIdleReason.CLEANING_SUCCEEDED:
        this.renderCleanupCard_(ChromeCleanerCardState.CLEANUP_SUCCEEDED);
        break;

      case ChromeCleanupIdleReason.CLEANER_DOWNLOAD_FAILED:
        this.renderCleanupCard_(ChromeCleanerCardState.CLEANER_DOWNLOAD_FAILED);
        break;

      default:
        assert(false, `Unknown idle reason: ${idleReason}`);
    }
  }

  /**
   * Listener of event 'chrome-cleanup-on-scanning'.
   * No UI will be shown in the Settings page on that state, simply hide the
   * card and cleanup this element's fields.
   */
  private onScanning_() {
    this.ongoingAction_ = ChromeCleanupOngoingAction.SCANNING;
    this.scannerResults_ = this.emptyChromeCleanerScannerResults_;
    this.renderScanOfferedByDefault_ = false;
    this.renderCleanupCard_(ChromeCleanerCardState.SCANNING);
  }

  /**
   * Listener of event 'chrome-cleanup-on-infected'.
   * Offers a cleanup to the user and enables presenting files to be removed.
   * @param isPoweredByPartner If scanning results are provided by a partner's
   *     engine.
   * @param scannerResults The cleanup items to be presented to the user.
   */
  private onInfected_(
      isPoweredByPartner: boolean,
      scannerResults: ChromeCleanerScannerResults) {
    this.isPoweredByPartner_ = isPoweredByPartner;
    this.ongoingAction_ = ChromeCleanupOngoingAction.NONE;
    this.renderScanOfferedByDefault_ = false;
    this.scannerResults_ = scannerResults;
    this.updateShowItemsLinklabel_();
    this.renderCleanupCard_(ChromeCleanerCardState.CLEANUP_OFFERED);
  }

  /**
   * Listener of event 'chrome-cleanup-on-cleaning'.
   * Shows a spinner indicating that an on-going action and enables presenting
   * files to be removed.
   * @param isPoweredByPartner If scanning results are provided by a partner's
   *     engine.
   * @param scannerResults The cleanup items to be presented to the user.
   */
  private onCleaning_(
      isPoweredByPartner: boolean,
      scannerResults: ChromeCleanerScannerResults) {
    this.isPoweredByPartner_ = isPoweredByPartner;
    this.ongoingAction_ = ChromeCleanupOngoingAction.CLEANING;
    this.renderScanOfferedByDefault_ = false;
    this.scannerResults_ = scannerResults;
    this.updateShowItemsLinklabel_();
    this.renderCleanupCard_(ChromeCleanerCardState.CLEANING);
  }

  /**
   * Listener of event 'chrome-cleanup-on-reboot-required'.
   * No UI will be shown in the Settings page on that state, so we simply hide
   * the card and cleanup this element's fields.
   */
  private onRebootRequired_() {
    this.ongoingAction_ = ChromeCleanupOngoingAction.NONE;
    this.scannerResults_ = this.emptyChromeCleanerScannerResults_;
    this.renderScanOfferedByDefault_ = false;
    this.renderCleanupCard_(ChromeCleanerCardState.REBOOT_REQUIRED);
  }

  /**
   * Renders the cleanup card given the state and list of files.
   * @param state The card state to be rendered.
   */
  private renderCleanupCard_(state: ChromeCleanerCardState) {
    const components = this.cardStateToComponentsMap_!.get(state);
    assert(components);

    this.title_ = components.title === null ?
        window.trustedTypes!.emptyHTML :
        sanitizeInnerHtml(components.title);
    this.explanation_ = components.explanation || '';
    this.updateActionButton_(components.actionButton);
    this.updateCardFlags_(components.flags);
  }

  /**
   * Updates the action button on the cleanup card as the action expected for
   * the current state.
   * @param actionButton The button to render, or null if no button should be
   *     shown.
   */
  private updateActionButton_(actionButton: ChromeCleanupCardActionButton|
                              null) {
    if (!actionButton) {
      this.showActionButton_ = false;
      this.actionButtonLabel_ = '';
      this.doAction_ = null;
    } else {
      this.showActionButton_ = true;
      this.actionButtonLabel_ = actionButton.label;
      this.doAction_ = actionButton.doAction;
    }
  }

  /**
   * Updates boolean flags corresponding to optional components to be rendered
   * on the card.
   * @param flags Flags indicating optional components to be rendered.
   */
  private updateCardFlags_(flags: number) {
    this.showLogsPermission_ =
        (flags & ChromeCleanupCardFlags.SHOW_LOGS_PERMISSIONS) !== 0;
    this.isWaitingForResult_ =
        (flags & ChromeCleanupCardFlags.WAITING_FOR_RESULT) !== 0;
    this.showItemsToRemove_ =
        (flags & ChromeCleanupCardFlags.SHOW_ITEMS_TO_REMOVE) !== 0;

    // Files to remove list should only be expandable if details are being
    // shown, otherwise it will add extra padding at the bottom of the card.
    if (!this.showExplanation_ || !this.showItemsToRemove_) {
      this.itemsToRemoveSectionExpanded_ = false;
    }
  }

  /**
   * @param enabled Whether cleanup is enabled.
   */
  private onCleanupEnabledChange_(enabled: boolean) {
    this.cleanupEnabled_ = enabled;
  }

  /**
   * Sends an action to the browser proxy to start scanning.
   */
  private startScanning_() {
    this.browserProxy_.startScanning(
        this.$.chromeCleanupLogsUploadControl.checked);
  }

  /**
   * Sends an action to the browser proxy to start the cleanup.
   */
  private startCleanup_() {
    this.browserProxy_.startCleanup(
        this.$.chromeCleanupLogsUploadControl.checked);
  }

  /**
   * Sends an action to the browser proxy to restart the machine.
   */
  private restartComputer_() {
    this.browserProxy_.restartComputer();
  }

  /**
   * Updates the label for the collapsed detailed view. If user-initiated
   * cleanups are enabled, the string is obtained from the browser proxy,
   * since it may require a plural version. Otherwise, use the default value
   * for |chromeCleanupLinkShowItems|.
   */
  private updateShowItemsLinklabel_() {
    const setShowItemsLabel = (text: string) => this.showItemsLinkLabel_ = text;
    this.browserProxy_
        .getItemsToRemovePluralString(
            this.scannerResults_.files.length +
            this.scannerResults_.registryKeys.length)
        .then(setShowItemsLabel);
  }

  /**
   * @return The map of card states to components to be rendered.
   */
  private buildCardStateToComponentsMap_():
      Map<ChromeCleanerCardState, ChromeCleanupCardComponents> {
    /**
     * The action buttons to show on the card.
     * @enum {ChromeCleanupCardActionButton}
     */
    const actionButtons = {
      FIND: {
        label: this.i18n('chromeCleanupFindButtonLabel'),
        doAction: () => this.startScanning_(),
      },

      REMOVE: {
        label: this.i18n('chromeCleanupRemoveButtonLabel'),
        doAction: () => this.startCleanup_(),
      },

      RESTART_COMPUTER: {
        label: this.i18n('chromeCleanupRestartButtonLabel'),
        doAction: () => this.restartComputer_(),
      },

      TRY_SCAN_AGAIN: {
        label: this.i18n('chromeCleanupTitleTryAgainButtonLabel'),
        // TODO(crbug.com/776538): do not run the reporter component again.
        // Try downloading the cleaner and scan with it instead.
        doAction: () => this.startScanning_(),
      },
    };

    return new Map([
      [
        ChromeCleanerCardState.CLEANUP_OFFERED,
        {
          title: this.i18n('chromeCleanupTitleRemove'),
          explanation: this.i18n('chromeCleanupExplanationRemove'),
          actionButton: actionButtons.REMOVE,
          flags: ChromeCleanupCardFlags.SHOW_LOGS_PERMISSIONS |
              ChromeCleanupCardFlags.SHOW_ITEMS_TO_REMOVE,
        },
      ],
      [
        ChromeCleanerCardState.CLEANING,
        {
          title: this.i18n('chromeCleanupTitleRemoving'),
          explanation: this.i18n('chromeCleanupExplanationRemoving'),
          actionButton: null,
          flags: ChromeCleanupCardFlags.WAITING_FOR_RESULT |
              ChromeCleanupCardFlags.SHOW_ITEMS_TO_REMOVE,
        },
      ],
      [
        ChromeCleanerCardState.REBOOT_REQUIRED,
        {
          title: this.i18n('chromeCleanupTitleRestart'),
          explanation: null,
          actionButton: actionButtons.RESTART_COMPUTER,
          flags: ChromeCleanupCardFlags.NONE,
        },
      ],
      [
        ChromeCleanerCardState.CLEANUP_SUCCEEDED,
        {
          title: this.i18nAdvanced('chromeCleanupTitleRemoved', {tags: ['a']})
                     .toString(),
          explanation: null,
          actionButton: null,
          flags: ChromeCleanupCardFlags.NONE,
        },
      ],
      [
        ChromeCleanerCardState.CLEANING_FAILED,
        {
          title: this.i18n('chromeCleanupTitleErrorCantRemove'),
          explanation: this.i18n('chromeCleanupExplanationCleanupError'),
          actionButton: null,
          flags: ChromeCleanupCardFlags.NONE,
        },
      ],
      [
        ChromeCleanerCardState.SCANNING_OFFERED,
        {
          title: this.i18n('chromeCleanupTitleFindAndRemove'),
          explanation: this.i18n('chromeCleanupExplanationFindAndRemove'),
          actionButton: actionButtons.FIND,
          flags: ChromeCleanupCardFlags.SHOW_LOGS_PERMISSIONS,
        },
      ],
      [
        ChromeCleanerCardState.SCANNING,
        {
          title: this.i18n('chromeCleanupTitleScanning'),
          explanation: null,
          actionButton: null,
          flags: ChromeCleanupCardFlags.WAITING_FOR_RESULT,
        },
      ],
      [
        // TODO(crbug.com/776538): Could we offer to reset settings here?
        ChromeCleanerCardState.SCANNING_FOUND_NOTHING,
        {
          title: this.i18n('chromeCleanupTitleNothingFound'),
          explanation: null,
          actionButton: null,
          flags: ChromeCleanupCardFlags.NONE,
        },
      ],
      [
        ChromeCleanerCardState.SCANNING_FAILED,
        {
          title: this.i18n('chromeCleanupTitleScanningFailed'),
          explanation: this.i18n('chromeCleanupExplanationScanError'),
          actionButton: null,
          flags: ChromeCleanupCardFlags.NONE,
        },
      ],
      [
        ChromeCleanerCardState.CLEANER_DOWNLOAD_FAILED,
        {
          // TODO(crbug.com/776538): distinguish between missing network
          // connectivity and cleanups being disabled by the server.
          title: this.i18n('chromeCleanupTitleCleanupUnavailable'),
          explanation: this.i18n('chromeCleanupExplanationCleanupUnavailable'),
          actionButton: actionButtons.TRY_SCAN_AGAIN,
          flags: ChromeCleanupCardFlags.NONE,
        },
      ],
    ]);
  }

  private getListEntriesFromStrings_(list: string[]):
      ChromeCleanupRemovalListItem[] {
    return list.map(entry => ({text: entry, highlightSuffix: null}));
  }

  private getListEntriesFromFilePaths_(paths: ChromeCleanupFilePath[]):
      ChromeCleanupRemovalListItem[] {
    return paths.map(
        path => ({text: path.dirname, highlightSuffix: path.basename}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-chrome-cleanup-page': SettingsChromeCleanupPageElement;
  }
}

customElements.define(
    SettingsChromeCleanupPageElement.is, SettingsChromeCleanupPageElement);

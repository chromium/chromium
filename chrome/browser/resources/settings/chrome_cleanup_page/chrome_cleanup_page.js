// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The reason why the controller is in state kIdle.
 * Must be kept in sync with ChromeCleanerController::IdleReason.
 * @enum {string}
 */
settings.ChromeCleanupIdleReason = {
  INITIAL: 'initial',
  REPORTER_FOUND_NOTHING: 'reporter_found_nothing',
  REPORTER_FAILED: 'reporter_failed',
  SCANNING_FOUND_NOTHING: 'scanning_found_nothing',
  SCANNING_FAILED: 'scanning_failed',
  CONNECTION_LOST: 'connection_lost',
  USER_DECLINED_CLEANUP: 'user_declined_cleanup',
  CLEANING_FAILED: 'cleaning_failed',
  CLEANING_SUCCEEDED: 'cleaning_succeeded',
  CLEANER_DOWNLOAD_FAILED: 'cleaner_download_failed',
};

/**
 * The possible states for the cleanup card.
 * @enum {string}
 */
settings.ChromeCleanerCardState = {
  SCANNING_OFFERED: 'scanning_offered',
  SCANNING: 'scanning',
  CLEANUP_OFFERED: 'cleanup_offered',
  CLEANING: 'cleaning',
  REBOOT_REQUIRED: 'reboot_required',
  SCANNING_FOUND_NOTHING: 'scanning_found_nothing',
  SCANNING_FAILED: 'scanning_failed',
  CLEANUP_SUCCEEDED: 'cleanup_succeeded',
  CLEANING_FAILED: 'cleanup_failed',
  CLEANER_DOWNLOAD_FAILED: 'cleaner_download_failed',
};

/**
 * Boolean properties for a cleanup card state.
 * @enum {number}
 */
settings.ChromeCleanupCardFlags = {
  NONE: 0,
  SHOW_LOGS_PERMISSIONS: 1 << 0,
  WAITING_FOR_RESULT: 1 << 1,
  SHOW_ITEMS_TO_REMOVE: 1 << 2,
};

/**
 * Identifies an ongoing scanning/cleanup action.
 * @enum {number}
 */
settings.ChromeCleanupOngoingAction = {
  NONE: 0,
  SCANNING: 1,
  CLEANING: 2,
};

/**
 * @typedef {{
 *   label: string,
 *   doAction: !function(),
 * }}
 */
settings.ChromeCleanupCardActionButton;

/**
 * @typedef {{
 *   title: ?string,
 *   explanation: ?string,
 *   actionButton: ?settings.ChromeCleanupCardActionButton,
 *   flags: number,
 * }}
 */
settings.ChromeCleanupCardComponents;

/**
 * Represents the file path structure of a base::FilePath.
 * dirname ends with a separator.
 * @typedef {{
 *   dirname: string,
 *   basename: string,
 * }}
 */
settings.ChromeCleanupFilePath;

/**
 * @typedef {{
 *   files: Array<settings.ChromeCleanupFilePath>,
 *   registryKeys: Array<string>,
 *   extensions: Array<string>,
 * }}
 */
settings.ChromeCleanerScannerResults;

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
Polymer({
  is: 'settings-chrome-cleanup-page',

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
    /** Preferences state. */
    prefs: Object,

    /** @private */
    title_: {
      type: String,
      value: '',
    },

    /** @private */
    explanation_: {
      type: String,
      value: '',
    },

    /** @private */
    isWaitingForResult_: {
      type: Boolean,
      value: '',
    },

    /** @private */
    showActionButton_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    cleanupEnabled_: {
      type: Boolean,
      value: true,
    },

    /** @private */
    actionButtonLabel_: {
      type: String,
      value: '',
    },

    /** @private */
    showExplanation_: {
      type: Boolean,
      computed: 'computeShowExplanation_(explanation_)',
    },

    /** @private */
    showLogsPermission_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    showItemsToRemove_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    itemsToRemoveSectionExpanded_: {
      type: Boolean,
      value: false,
      observer: 'itemsToRemoveSectionExpandedChanged_',
    },

    /** @private */
    showItemsLinkLabel_: {
      type: String,
      value: '',
    },

    /** @private */
    showingAllFiles_: {
      type: Boolean,
      value: false,
    },

    /** @private {!settings.ChromeCleanerScannerResults} */
    scannerResults_: {
      type: Array,
      value: function() {
        return {'files': [], 'registryKeys': [], 'extensions': []};
      },
    },

    /** @private */
    hasFilesToShow_: {
      type: Boolean,
      computed: 'computeHasFilesToShow_(scannerResults_)',
    },

    /** @private */
    hasRegistryKeysToShow_: {
      type: Boolean,
      computed: 'computeHasRegistryKeysToShow_(scannerResults_)',
    },

    /** @private */
    hasExtensionsToShow_: {
      type: Boolean,
      computed: 'computeHasExtensionsToShow_(scannerResults_)',
    },

    /** @private {chrome.settingsPrivate.PrefObject} */
    logsUploadPref_: {
      type: Object,
      value: function() {
        return /** @type {chrome.settingsPrivate.PrefObject} */ ({});
      },
    },

    /** @private */
    isPoweredByPartner_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private {!settings.ChromeCleanerScannerResults} */
  emptyChromeCleanerScannerResults_:
      {'files': [], 'registryKeys': [], 'extensions': []},

  /** @private {?settings.ChromeCleanupProxy} */
  browserProxy_: null,

  /** @private {?function()} */
  doAction_: null,

  /** @private {?Map<settings.ChromeCleanerCardState,
   *                 !settings.ChromeCleanupCardComponents>} */
  cardStateToComponentsMap_: null,

  /** @private {settings.ChromeCleanupOngoingAction} */
  ongoingAction_: settings.ChromeCleanupOngoingAction.NONE,

  /**
   * If true, the scan offered view is rendered on state idle, regardless of
   * the idle reason received from the cleaner controller. The goal is to
   * ignore previous interactions (such as completed cleanups) performed on
   * other tabs or if this tab is reloaded.
   * Set to false whenever there is a transition to a non-idle state while the
   * current tab is open.
   * @private {boolean}
   */
  renderScanOfferedByDefault_: true,

  /** @override */
  attached: function() {
    this.browserProxy_ = settings.ChromeCleanupProxyImpl.getInstance();
    this.cardStateToComponentsMap_ = this.buildCardStateToComponentsMap_();

    this.addWebUIListener('chrome-cleanup-on-idle', this.onIdle_.bind(this));
    this.addWebUIListener(
        'chrome-cleanup-on-scanning', this.onScanning_.bind(this));
    // Note: both reporter running and scanning share the same UI.
    this.addWebUIListener(
        'chrome-cleanup-on-reporter-running', this.onScanning_.bind(this));
    this.addWebUIListener(
        'chrome-cleanup-on-infected', this.onInfected_.bind(this));
    this.addWebUIListener(
        'chrome-cleanup-on-cleaning', this.onCleaning_.bind(this));
    this.addWebUIListener(
        'chrome-cleanup-on-reboot-required', this.onRebootRequired_.bind(this));
    this.addWebUIListener(
        'chrome-cleanup-upload-permission-change',
        this.onUploadPermissionChange_.bind(this));
    this.addWebUIListener(
        'chrome-cleanup-enabled-change',
        this.onCleanupEnabledChange_.bind(this));
    this.browserProxy_.registerChromeCleanerObserver();
  },

  /**
   * Implements the action for the only visible button in the UI, which can be
   * either to start an action such as a cleanup or to restart the computer.
   * @private
   */
  proceed_: function() {
    this.doAction_();
  },

  getTopSettingsBoxClass_: function(showDetails) {
    return showDetails ? 'top-aligned-settings-box' : 'two-line';
  },

  /**
   * Toggles the expand button within the element being listened to.
   * @param {!Event} e
   * @private
   */
  toggleExpandButton_: function(e) {
    // The expand button handles toggling itself.
    const expandButtonTag = 'CR-EXPAND-BUTTON';
    if (e.target.tagName == expandButtonTag)
      return;

    /** @type {!CrExpandButtonElement} */
    const expandButton = e.currentTarget.querySelector(expandButtonTag);
    assert(expandButton);
    expandButton.expanded = !expandButton.expanded;
  },

  /**
   * Notifies Chrome that the details section was opened or closed.
   * @private
   */
  itemsToRemoveSectionExpandedChanged_: function(newVal, oldVal) {
    if (!oldVal && newVal)
      this.browserProxy_.notifyShowDetails(this.itemsToRemoveSectionExpanded_);
  },

  /**
   * @param {string} explanation
   * @return {boolean}
   * @private
   */
  computeShowExplanation_: function(explanation) {
    return explanation != '';
  },

  /**
   * Returns true if there are files to show to the user.
   * @param {!settings.ChromeCleanerScannerResults} scannerResults The cleanup
   *     items to be presented to the user.
   * @return {boolean}
   * @private
   */
  computeHasFilesToShow_(scannerResults) {
    return scannerResults.files.length > 0;
  },

  /**
   * Returns true if user-initiated cleanups are enabled and there are registry
   * keys to show to the user.
   * @param {!settings.ChromeCleanerScannerResults} scannerResults The cleanup
   *     items to be presented to the user.
   * @return {boolean}
   * @private
   */
  computeHasRegistryKeysToShow_(scannerResults) {
    return scannerResults.registryKeys.length > 0;
  },

  /**
   * Returns true if user-initiated cleanups are enabled and there are
   * extensions to show to the user.
   * @param {!settings.ChromeCleanerScannerResults} scannerResults The cleanup
   *     items to be presented to the user.
   * @return {boolean}
   * @private
   */
  computeHasExtensionsToShow_(scannerResults) {
    return scannerResults.extensions.length > 0;
  },

  /**
   * Listener of event 'chrome-cleanup-on-idle'.
   * @param {string} idleReason
   * @private
   */
  onIdle_: function(idleReason) {
    this.ongoingAction_ = settings.ChromeCleanupOngoingAction.NONE;
    this.scannerResults_ = this.emptyChromeCleanerScannerResults_;

    // Ignore the idle reason and render the scan offered view if no
    // interaction happened on this tab.
    if (this.renderScanOfferedByDefault_) {
      idleReason = settings.ChromeCleanupIdleReason.INITIAL;
    }

    switch (idleReason) {
      case settings.ChromeCleanupIdleReason.INITIAL:
        this.renderCleanupCard_(
            settings.ChromeCleanerCardState.SCANNING_OFFERED);
        break;

      case settings.ChromeCleanupIdleReason.SCANNING_FOUND_NOTHING:
      case settings.ChromeCleanupIdleReason.REPORTER_FOUND_NOTHING:
        this.renderCleanupCard_(
            settings.ChromeCleanerCardState.SCANNING_FOUND_NOTHING);
        break;

      case settings.ChromeCleanupIdleReason.SCANNING_FAILED:
      case settings.ChromeCleanupIdleReason.REPORTER_FAILED:
        this.renderCleanupCard_(
            settings.ChromeCleanerCardState.SCANNING_FAILED);
        break;

      case settings.ChromeCleanupIdleReason.CONNECTION_LOST:
        if (this.ongoingAction_ ==
            settings.ChromeCleanupOngoingAction.SCANNING) {
          this.renderCleanupCard_(
              settings.ChromeCleanerCardState.SCANNING_FAILED);
        } else {
          assert(
              this.ongoingAction_ ==
              settings.ChromeCleanupOngoingAction.CLEANING);
          this.renderCleanupCard_(
              settings.ChromeCleanerCardState.CLEANING_FAILED);
        }
        break;

      case settings.ChromeCleanupIdleReason.CLEANING_FAILED:
      case settings.ChromeCleanupIdleReason.USER_DECLINED_CLEANUP:
        this.renderCleanupCard_(
            settings.ChromeCleanerCardState.CLEANING_FAILED);
        break;

      case settings.ChromeCleanupIdleReason.CLEANING_SUCCEEDED:
        this.renderCleanupCard_(
            settings.ChromeCleanerCardState.CLEANUP_SUCCEEDED);
        break;

      case settings.ChromeCleanupIdleReason.CLEANER_DOWNLOAD_FAILED:
        this.renderCleanupCard_(
            settings.ChromeCleanerCardState.CLEANER_DOWNLOAD_FAILED);
        break;

      default:
        assert(false, `Unknown idle reason: ${idleReason}`);
    }
  },

  /**
   * Listener of event 'chrome-cleanup-on-scanning'.
   * No UI will be shown in the Settings page on that state, simply hide the
   * card and cleanup this element's fields.
   * @private
   */
  onScanning_: function() {
    this.ongoingAction_ = settings.ChromeCleanupOngoingAction.SCANNING;
    this.scannerResults_ = this.emptyChromeCleanerScannerResults_;
    this.renderScanOfferedByDefault_ = false;
    this.renderCleanupCard_(settings.ChromeCleanerCardState.SCANNING);
  },

  /**
   * Listener of event 'chrome-cleanup-on-infected'.
   * Offers a cleanup to the user and enables presenting files to be removed.
   * @param {boolean} isPoweredByPartner If scanning results are provided by a
   *     partner's engine.
   * @param {!settings.ChromeCleanerScannerResults} scannerResults The cleanup
   *     items to be presented to the user.
   * @private
   */
  onInfected_: function(isPoweredByPartner, scannerResults) {
    this.isPoweredByPartner_ = isPoweredByPartner;
    this.ongoingAction_ = settings.ChromeCleanupOngoingAction.NONE;
    this.renderScanOfferedByDefault_ = false;
    this.scannerResults_ = scannerResults;
    this.updateShowItemsLinklabel_();
    this.renderCleanupCard_(settings.ChromeCleanerCardState.CLEANUP_OFFERED);
  },

  /**
   * Listener of event 'chrome-cleanup-on-cleaning'.
   * Shows a spinner indicating that an on-going action and enables presenting
   * files to be removed.
   * @param {boolean} isPoweredByPartner If scanning results are provided by a
   *     partner's engine.
   * @param {!settings.ChromeCleanerScannerResults} scannerResults The cleanup
   *     items to be presented to the user.
   * @private
   */
  onCleaning_: function(isPoweredByPartner, scannerResults) {
    this.isPoweredByPartner_ = isPoweredByPartner;
    this.ongoingAction_ = settings.ChromeCleanupOngoingAction.CLEANING;
    this.renderScanOfferedByDefault_ = false;
    this.scannerResults_ = scannerResults;
    this.updateShowItemsLinklabel_();
    this.renderCleanupCard_(settings.ChromeCleanerCardState.CLEANING);
  },

  /**
   * Listener of event 'chrome-cleanup-on-reboot-required'.
   * No UI will be shown in the Settings page on that state, so we simply hide
   * the card and cleanup this element's fields.
   * @private
   */
  onRebootRequired_: function() {
    this.ongoingAction_ = settings.ChromeCleanupOngoingAction.NONE;
    this.scannerResults_ = this.emptyChromeCleanerScannerResults_;
    this.renderScanOfferedByDefault_ = false;
    this.renderCleanupCard_(settings.ChromeCleanerCardState.REBOOT_REQUIRED);
  },

  /**
   * Renders the cleanup card given the state and list of files.
   * @param {!settings.ChromeCleanerCardState} state The card state to be
   *     rendered.
   * @private
   */
  renderCleanupCard_: function(state) {
    const components = this.cardStateToComponentsMap_.get(state);
    assert(components);

    this.title_ = components.title || '';
    this.explanation_ = components.explanation || '';
    this.updateActionButton_(components.actionButton);
    this.updateCardFlags_(components.flags);
  },

  /**
   * Updates the action button on the cleanup card as the action expected for
   * the current state.
   * @param {?settings.ChromeCleanupCardActionButton} actionButton
   *     The button to render, or null if no button should be shown.
   * @private
   */
  updateActionButton_: function(actionButton) {
    if (!actionButton) {
      this.showActionButton_ = false;
      this.actionButtonLabel_ = '';
      this.doAction_ = null;
    } else {
      this.showActionButton_ = true;
      this.actionButtonLabel_ = actionButton.label;
      this.doAction_ = actionButton.doAction;
    }
  },

  /**
   * Updates boolean flags corresponding to optional components to be rendered
   * on the card.
   * @param {number} flags Flags indicating optional components to be rendered.
   * @private
   */
  updateCardFlags_: function(flags) {
    this.showLogsPermission_ =
        (flags & settings.ChromeCleanupCardFlags.SHOW_LOGS_PERMISSIONS) != 0;
    this.isWaitingForResult_ =
        (flags & settings.ChromeCleanupCardFlags.WAITING_FOR_RESULT) != 0;
    this.showItemsToRemove_ =
        (flags & settings.ChromeCleanupCardFlags.SHOW_ITEMS_TO_REMOVE) != 0;

    // Files to remove list should only be expandable if details are being
    // shown, otherwise it will add extra padding at the bottom of the card.
    if (!this.showExplanation_ || !this.showItemsToRemove_)
      this.itemsToRemoveSectionExpanded_ = false;
  },

  /**
   * @param {boolean} managed Whether uploads are controlled by policy or not.
   * @param {boolean} enabled Whether logs upload is enabled.
   * @private
   */
  onUploadPermissionChange_: function(managed, enabled) {
    const pref = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: enabled,
    };
    if (managed) {
      pref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      pref.controlledBy = chrome.settingsPrivate.ControlledBy.USER_POLICY;
    }
    this.logsUploadPref_ = pref;
  },

  /**
   * @param {boolean} enabled Whether cleanup is enabled.
   * @private
   */
  onCleanupEnabledChange_: function(enabled) {
    this.cleanupEnabled_ = enabled;
  },

  /** @private */
  changeLogsPermission_: function() {
    const enabled = this.$.chromeCleanupLogsUploadControl.checked;
    this.browserProxy_.setLogsUploadPermission(enabled);
  },

  /**
   * Sends an action to the browser proxy to start scanning.
   * @private
   */
  startScanning_: function() {
    this.browserProxy_.startScanning(
        this.$.chromeCleanupLogsUploadControl.checked);
  },

  /**
   * Sends an action to the browser proxy to start the cleanup.
   * @private
   */
  startCleanup_: function() {
    this.browserProxy_.startCleanup(
        this.$.chromeCleanupLogsUploadControl.checked);
  },

  /**
   * Sends an action to the browser proxy to restart the machine.
   * @private
   */
  restartComputer_: function() {
    this.browserProxy_.restartComputer();
  },

  /**
   * Updates the label for the collapsed detailed view. If user-initiated
   * cleanups are enabled, the string is obtained from the browser proxy, since
   * it may require a plural version. Otherwise, use the default value for
   * |chromeCleanupLinkShowItems|.
   */
  updateShowItemsLinklabel_: function() {
    const setShowItemsLabel = text => this.showItemsLinkLabel_ = text;
    this.browserProxy_
        .getItemsToRemovePluralString(
            this.scannerResults_.files.length +
            this.scannerResults_.registryKeys.length +
            this.scannerResults_.extensions.length)
        .then(setShowItemsLabel);
  },

  /**
   * Returns the map of card states to components to be rendered.
   * @return {!Map<settings.ChromeCleanerCardState,
   *               !settings.ChromeCleanupCardComponents>}
   * @private
   */
  buildCardStateToComponentsMap_: function() {
    /**
     * The action buttons to show on the card.
     * @enum {settings.ChromeCleanupCardActionButton}
     */
    const actionButtons = {
      FIND: {
        label: this.i18n('chromeCleanupFindButtonLable'),
        doAction: this.startScanning_.bind(this),
      },

      REMOVE: {
        label: this.i18n('chromeCleanupRemoveButtonLabel'),
        doAction: this.startCleanup_.bind(this),
      },

      RESTART_COMPUTER: {
        label: this.i18n('chromeCleanupRestartButtonLabel'),
        doAction: this.restartComputer_.bind(this),
      },

      TRY_SCAN_AGAIN: {
        label: this.i18n('chromeCleanupTitleTryAgainButtonLabel'),
        // TODO(crbug.com/776538): do not run the reporter component again.
        // Try downloading the cleaner and scan with it instead.
        doAction: this.startScanning_.bind(this),
      },
    };

    return new Map([
      [
        settings.ChromeCleanerCardState.CLEANUP_OFFERED, {
          title: this.i18n('chromeCleanupTitleRemove'),
          explanation: this.i18n('chromeCleanupExplanationRemove'),
          actionButton: actionButtons.REMOVE,
          flags: settings.ChromeCleanupCardFlags.SHOW_LOGS_PERMISSIONS |
              settings.ChromeCleanupCardFlags.SHOW_ITEMS_TO_REMOVE,
        }
      ],
      [
        settings.ChromeCleanerCardState.CLEANING, {
          title: this.i18n('chromeCleanupTitleRemoving'),
          explanation: this.i18n('chromeCleanupExplanationRemoving'),
          actionButton: null,
          flags: settings.ChromeCleanupCardFlags.WAITING_FOR_RESULT |
              settings.ChromeCleanupCardFlags.SHOW_ITEMS_TO_REMOVE,
        }
      ],
      [
        settings.ChromeCleanerCardState.REBOOT_REQUIRED, {
          title: this.i18n('chromeCleanupTitleRestart'),
          explanation: null,
          actionButton: actionButtons.RESTART_COMPUTER,
          flags: settings.ChromeCleanupCardFlags.NONE,
        }
      ],
      [
        settings.ChromeCleanerCardState.CLEANUP_SUCCEEDED, {
          title: this.i18nAdvanced('chromeCleanupTitleRemoved', {tags: ['a']}),
          explanation: null,
          actionButton: null,
          flags: settings.ChromeCleanupCardFlags.NONE,
        }
      ],
      [
        settings.ChromeCleanerCardState.CLEANING_FAILED, {
          title: this.i18n('chromeCleanupTitleErrorCantRemove'),
          explanation: this.i18n('chromeCleanupExplanationCleanupError'),
          actionButton: null,
          flags: settings.ChromeCleanupCardFlags.NONE,
        }
      ],
      [
        settings.ChromeCleanerCardState.SCANNING_OFFERED, {
          title: this.i18n('chromeCleanupTitleFindAndRemove'),
          explanation: this.i18n('chromeCleanupExplanationFindAndRemove'),
          actionButton: actionButtons.FIND,
          flags: settings.ChromeCleanupCardFlags.SHOW_LOGS_PERMISSIONS,
        }
      ],
      [
        settings.ChromeCleanerCardState.SCANNING, {
          title: this.i18n('chromeCleanupTitleScanning'),
          explanation: null,
          actionButton: null,
          flags: settings.ChromeCleanupCardFlags.WAITING_FOR_RESULT,
        }
      ],
      [
        // TODO(crbug.com/776538): Could we offer to reset settings here?
        settings.ChromeCleanerCardState.SCANNING_FOUND_NOTHING, {
          title: this.i18n('chromeCleanupTitleNothingFound'),
          explanation: null,
          actionButton: null,
          flags: settings.ChromeCleanupCardFlags.NONE,
        }
      ],
      [
        settings.ChromeCleanerCardState.SCANNING_FAILED, {
          title: this.i18n('chromeCleanupTitleScanningFailed'),
          explanation: this.i18n('chromeCleanupExplanationScanError'),
          actionButton: null,
          flags: settings.ChromeCleanupCardFlags.NONE,
        }
      ],
      [
        settings.ChromeCleanerCardState.CLEANER_DOWNLOAD_FAILED,
        {
          // TODO(crbug.com/776538): distinguish between missing network
          // connectivity and cleanups being disabled by the server.
          title: this.i18n('chromeCleanupTitleCleanupUnavailable'),
          explanation: this.i18n('chromeCleanupExplanationCleanupUnavailable'),
          actionButton: actionButtons.TRY_SCAN_AGAIN,
          flags: settings.ChromeCleanupCardFlags.NONE,
        },
      ],
    ]);
  },

  /**
   * @param {!Array<string>} list
   * @return {!Array<settings.ChromeCleanupRemovalListItem>}
   * @private
   */
  getListEntriesFromStrings_: function(list) {
    return list.map(entry => ({text: entry, highlightSuffix: null}));
  },

  /**
   * @param {!Array<settings.ChromeCleanupFilePath>} paths
   * @return {!Array<settings.ChromeCleanupRemovalListItem>}
   * @private
   */
  getListEntriesFromFilePaths_: function(paths) {
    return paths.map(
        path => ({text: path.dirname, highlightSuffix: path.basename}));
  },
});

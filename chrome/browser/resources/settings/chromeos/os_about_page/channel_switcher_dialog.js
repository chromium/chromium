// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

/**
 */
const WarningMessage = {
  NONE: -1,
  ENTERPRISE_MANAGED: 0,
  POWERWASH: 1,
  UNSTABLE: 2,
};

/**
 * @fileoverview 'settings-channel-switcher-dialog' is a component allowing the
 * user to switch between release channels (dev, beta, stable). A
 * |target-channel-changed| event is fired if the user does select a different
 * release channel to notify parents of this dialog.
 */
Polymer({
  is: 'settings-channel-switcher-dialog',

  properties: {
    /** @private */
    browserChannelEnum_: {
      type: Object,
      value: BrowserChannel,
    },

    /** @private {!BrowserChannel} */
    currentChannel_: String,

    /** @private {!BrowserChannel} */
    targetChannel_: String,

    /**
     * Controls which of the two action buttons is visible.
     * @private {?{changeChannel: boolean, changeChannelAndPowerwash: boolean}}
     */
    shouldShowButtons_: {
      type: Object,
      value: null,
    },
  },

  /** @private {?settings.AboutPageBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready() {
    this.browserProxy_ = settings.AboutPageBrowserProxyImpl.getInstance();
    this.browserProxy_.getChannelInfo().then(info => {
      this.currentChannel_ = info.currentChannel;
      this.targetChannel_ = info.targetChannel;
      // Pre-populate radio group with target channel.
      const radioGroup = this.$$('cr-radio-group');
      radioGroup.selected = this.targetChannel_;
      radioGroup.focus();
    });
  },

  /** @override */
  attached() {
    this.$.dialog.showModal();
  },

  /** @private */
  onCancelTap_() {
    this.$.dialog.close();
  },

  /** @private */
  onChangeChannelTap_() {
    const selectedChannel = this.$$('cr-radio-group').selected;
    this.browserProxy_.setChannel(selectedChannel, false);
    this.$.dialog.close();
    this.fire('target-channel-changed', selectedChannel);
  },

  /** @private */
  onChangeChannelAndPowerwashTap_() {
    const selectedChannel = this.$$('cr-radio-group').selected;
    this.browserProxy_.setChannel(selectedChannel, true);
    this.$.dialog.close();
    this.fire('target-channel-changed', selectedChannel);
  },

  /**
   * @param {boolean} changeChannel Whether the changeChannel button should be
   *     visible.
   * @param {boolean} changeChannelAndPowerwash Whether the
   *     changeChannelAndPowerwash button should be visible.
   * @private
   */
  updateButtons_(changeChannel, changeChannelAndPowerwash) {
    if (changeChannel || changeChannelAndPowerwash) {
      // Ensure that at most one button is visible at any given time.
      assert(changeChannel !== changeChannelAndPowerwash);
    }

    this.shouldShowButtons_ = {
      changeChannel: changeChannel,
      changeChannelAndPowerwash: changeChannelAndPowerwash,
    };
  },

  /** @private */
  onChannelSelectionChanged_() {
    const selectedChannel = this.$$('cr-radio-group').selected;

    // Selected channel is the same as the target channel so only show 'cancel'.
    if (selectedChannel === this.targetChannel_) {
      this.shouldShowButtons_ = null;
      this.$.warningSelector.select(WarningMessage.NONE);
      return;
    }

    // Selected channel is the same as the current channel, allow the user to
    // change without warnings.
    if (selectedChannel === this.currentChannel_) {
      this.updateButtons_(true, false);
      this.$.warningSelector.select(WarningMessage.NONE);
      return;
    }

    if (settings.isTargetChannelMoreStable(
            this.currentChannel_, selectedChannel)) {
      // More stable channel selected. For non managed devices, notify the user
      // about powerwash.
      if (loadTimeData.getBoolean('aboutEnterpriseManaged')) {
        this.$.warningSelector.select(WarningMessage.ENTERPRISE_MANAGED);
        this.updateButtons_(true, false);
      } else {
        this.$.warningSelector.select(WarningMessage.POWERWASH);
        this.updateButtons_(false, true);
      }
    } else {
      if (selectedChannel === BrowserChannel.DEV) {
        // Dev channel selected, warn the user.
        this.$.warningSelector.select(WarningMessage.UNSTABLE);
      } else {
        this.$.warningSelector.select(WarningMessage.NONE);
      }
      this.updateButtons_(true, false);
    }
  },

  /**
   * @param {string} format
   * @param {string} replacement
   * @return {string}
   * @private
   */
  substituteString_(format, replacement) {
    return loadTimeData.substituteString(format, replacement);
  },
});
})();
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'chooser-exception-list' shows a list of chooser exceptions for a given
 * chooser type.
 */
Polymer({
  is: 'chooser-exception-list',

  behaviors: [
    I18nBehavior,
    ListPropertyUpdateBehavior,
    SiteSettingsBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * Array of chooser exceptions to display in the widget.
     * @type {!Array<ChooserException>}
     */
    chooserExceptions: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * The string ID of the chooser type that this element is displaying data
     * for.
     * See site_settings/constants.js for possible values.
     * @type {!settings.ChooserType}
     */
    chooserType: {
      observer: 'chooserTypeChanged_',
      type: String,
      value: settings.ChooserType.NONE,
    },

    /** @private */
    emptyListMessage_: {
      type: String,
      value: '',
    },

    /** @private */
    hasIncognito_: Boolean,

    /** @private */
    tooltipText_: String,
  },

  /** @override */
  created: function() {
    this.browserProxy_ =
        settings.SiteSettingsPrefsBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'contentSettingChooserPermissionChanged',
        this.objectWithinChooserTypeChanged_.bind(this));
    this.addWebUIListener(
        'onIncognitoStatusChanged', this.onIncognitoStatusChanged_.bind(this));
    this.browserProxy.updateIncognitoStatus();
  },

  /**
   * Called when a chooser exception changes permission and updates the element
   * if |category| is equal to the settings category of this element.
   * @param {settings.ContentSettingsTypes} category The content settings type
   *     that represents this permission category.
   * @param {settings.ChooserType} chooserType The content settings type that
   *     represents the chooser data for this permission.
   * @private
   */
  objectWithinChooserTypeChanged_: function(category, chooserType) {
    if (category === this.category && chooserType === this.chooserType) {
      this.chooserTypeChanged_();
    }
  },

  /**
   * Called for each chooser-exception-list when incognito is enabled or
   * disabled. Only called on change (opening N incognito windows only fires one
   * message). Another message is sent when the *last* incognito window closes.
   * @private
   */
  onIncognitoStatusChanged_: function(hasIncognito) {
    this.hasIncognito_ = hasIncognito;
    this.populateList_();
  },

  /**
   * Configures the visibility of the widget and shows the list.
   * @private
   */
  chooserTypeChanged_: function() {
    if (this.chooserType == settings.ChooserType.NONE) {
      return;
    }

    // Set the message to display when the exception list is empty.
    switch (this.chooserType) {
      case settings.ChooserType.USB_DEVICES:
        this.emptyListMessage_ = this.i18n('noUsbDevicesFound');
        break;
      case settings.ChooserType.SERIAL_PORTS:
        this.emptyListMessage_ = this.i18n('noSerialPortsFound');
        break;
      default:
        this.emptyListMessage_ = '';
    }

    this.populateList_();
  },

  /**
   * Returns true if there are any chooser exceptions for this chooser type.
   * @return {boolean}
   * @private
   */
  hasExceptions_: function() {
    return this.chooserExceptions.length > 0;
  },

  /**
   * Need to use a common tooltip since the tooltip in the entry is cut off from
   * the iron-list.
   * @param{!CustomEvent<!{target: HTMLElement, text: string}>} e
   * @private
   */
  onShowTooltip_: function(e) {
    this.tooltipText_ = e.detail.text;
    const target = e.detail.target;
    // paper-tooltip normally determines the target from the |for| property,
    // which is a selector. Here paper-tooltip is being reused by multiple
    // potential targets.
    this.$.tooltip.target = target;
    const hide = () => {
      this.$.tooltip.hide();
      target.removeEventListener('mouseleave', hide);
      target.removeEventListener('blur', hide);
      target.removeEventListener('tap', hide);
      this.$.tooltip.removeEventListener('mouseenter', hide);
    };
    target.addEventListener('mouseleave', hide);
    target.addEventListener('blur', hide);
    target.addEventListener('tap', hide);
    this.$.tooltip.addEventListener('mouseenter', hide);
    this.$.tooltip.show();
  },

  /**
   * Populate the chooser exception list for display.
   * @private
   */
  populateList_: function() {
    this.browserProxy_.getChooserExceptionList(this.chooserType)
        .then(exceptionList => this.processExceptions_(exceptionList));
  },

  /**
   * Process the chooser exception list returned from the native layer.
   * @param {!Array<RawChooserException>} exceptionList
   * @private
   */
  processExceptions_: function(exceptionList) {
    const exceptions = exceptionList.map(exception => {
      const sites = exception.sites.map(this.expandSiteException);
      return Object.assign(exception, {sites});
    });

    if (!this.updateList(
            'chooserExceptions', x => x.displayName, exceptions,
            true /* uidBasedUpdate */)) {
      // The chooser objects have not been changed, so check if their site
      // permissions have changed. The |exceptions| and |this.chooserExceptions|
      // arrays should be the same length.
      const siteUidGetter = x => x.origin + x.embeddingOrigin + x.incognito;
      exceptions.forEach((exception, index) => {
        const propertyPath = 'chooserExceptions.' + index + '.sites';
        this.updateList(propertyPath, siteUidGetter, exception.sites);
      }, this);
    }
  },
});

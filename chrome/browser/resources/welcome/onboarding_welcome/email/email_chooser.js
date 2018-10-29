// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('nuxEmail');

/**
 * @typedef {{
 *   id: number,
 *   name: string,
 *   icon: string,
 *   url: string,
 *   bookmarkId: (string|undefined),
 * }}
 */
nuxEmail.EmailProviderModel;

Polymer({
  is: 'email-chooser',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * @type {!Array<!nux.BookmarkListItem>}
     * @private
     */
    emailList_: Array,

    /** @private */
    bookmarkBarWasShown_: {
      type: Boolean,
      value: loadTimeData.getBoolean('bookmark_bar_shown'),
    },

    /** @private */
    finalized_: Boolean,

    /** @type {nux.stepIndicatorModel} */
    indicatorModel: Object,

    /** @private {?nuxEmail.EmailProviderModel} */
    selectedEmailProvider_: {
      type: Object,
      value: () => null,
      observer: 'onSelectedEmailProviderChange_',
    },
  },

  /** @private {nux.NuxEmailProxy} */
  emailProxy_: null,

  /** @private {nux.BookmarkProxy} */
  bookmarkProxy_: null,

  /** @override */
  attached: function() {
    Polymer.RenderStatus.afterNextRender(this, function() {
      Polymer.IronA11yAnnouncer.requestAvailability();
    });
  },

  /** @override */
  ready: function() {
    this.emailProxy_ = nux.NuxEmailProxyImpl.getInstance();
    this.bookmarkProxy_ = nux.BookmarkProxyImpl.getInstance();

    this.emailProxy_.recordPageInitialized();

    this.emailProxy_.getEmailList().then(list => {
      this.emailList_ = list;
    });

    window.addEventListener('beforeunload', () => {
      // Only need to clean up if user didn't interact with the buttons.
      if (this.finalized_)
        return;

      if (this.selectedEmailProvider_) {
        this.emailProxy_.recordProviderSelected(
            this.selectedEmailProvider_.id, this.emailList_.length);
      }

      this.emailProxy_.recordFinalize();
    });
  },

  /**
   * Handle toggling the email selected.
   * @param {!{model: {item: !nuxEmail.EmailProviderModel}}} e
   * @private
   */
  onEmailClick_: function(e) {
    if (this.getSelected_(e.model.item))
      this.selectedEmailProvider_ = null;
    else
      this.selectedEmailProvider_ = e.model.item;

    this.emailProxy_.recordClickedOption();
  },

  /**
   * @param {!Event} e
   * @private
   */
  onEmailPointerDown_: function(e) {
    e.currentTarget.classList.remove('keyboard-focused');
  },

  /**
   * @param {!Event} e
   * @private
   */
  onEmailKeyUp_: function(e) {
    e.currentTarget.classList.add('keyboard-focused');
  },

  /**
   * Returns whether |item| is selected or not.
   * @param {!nuxEmail.EmailProviderModel} item
   * @return boolean
   * @private
   */
  getSelected_: function(item) {
    return this.selectedEmailProvider_ &&
        item.name === this.selectedEmailProvider_.name;
  },

  /**
   * @param {nuxEmail.EmailProviderModel=} opt_emailProvider
   * @private
   */
  revertBookmark_: function(opt_emailProvider) {
    let emailProvider = opt_emailProvider || this.selectedEmailProvider_;

    if (emailProvider && emailProvider.bookmarkId)
      this.bookmarkProxy_.removeBookmark(emailProvider.bookmarkId);
  },

  /**
   * @param {nuxEmail.EmailProviderModel} newEmail
   * @param {nuxEmail.EmailProviderModel} prevEmail
   * @private
   */
  onSelectedEmailProviderChange_: function(newEmail, prevEmail) {
    if (!this.emailProxy_ || !this.bookmarkProxy_)
      return;

    if (prevEmail) {
      // If it was previously selected, it must've been assigned an id.
      assert(prevEmail.bookmarkId);
      this.revertBookmark_(prevEmail);
    }

    if (newEmail) {
      this.emailProxy_.cacheBookmarkIcon(newEmail.id);
      this.bookmarkProxy_.toggleBookmarkBar(true);
      this.bookmarkProxy_.addBookmark(
          {
            title: newEmail.name,
            url: newEmail.url,
            parentId: '1',
          },
          results => {
            this.selectedEmailProvider_.bookmarkId = results.id;
          });
    } else {
      this.bookmarkProxy_.toggleBookmarkBar(this.bookmarkBarWasShown_);
    }

    // Announcements are mutually exclusive, so keeping separate.
    if (prevEmail && newEmail) {
      this.fire('iron-announce', {text: this.i18n('bookmarkReplaced')});
    } else if (prevEmail) {
      this.fire('iron-announce', {text: this.i18n('bookmarkRemoved')});
    } else if (newEmail) {
      this.fire('iron-announce', {text: this.i18n('bookmarkAdded')});
    }
  },

  /** @private */
  onNoThanksClicked_: function() {
    this.finalized_ = true;
    this.revertBookmark_();
    this.bookmarkProxy_.toggleBookmarkBar(this.bookmarkBarWasShown_);
    this.emailProxy_.recordNoThanks();
    welcome.navigateToNextStep();
  },

  /** @private */
  onGetStartedClicked_: function() {
    this.finalized_ = true;
    this.emailProxy_.recordProviderSelected(
        this.selectedEmailProvider_.id, this.emailList_.length);
    this.emailProxy_.recordGetStarted();
    // TODO(scottchen): store the selected email provider URL somewhere to
    //     redirect to at the end.
    welcome.navigateToNextStep();
  },

  /** @private */
  onActionButtonClicked_: function() {
    if (this.$$('.action-button').disabled)
      this.emailProxy_.recordClickedDisabledButton();
  },
});

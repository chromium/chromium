// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-destination-dialog',

  behaviors: [I18nBehavior, ListPropertyUpdateBehavior],

  properties: {
    /** @type {?print_preview.DestinationStore} */
    destinationStore: {
      type: Object,
      observer: 'onDestinationStoreSet_',
    },

    /** @type {?print_preview.InvitationStore} */
    invitationStore: {
      type: Object,
      observer: 'onInvitationStoreSet_',
    },

    /** @private {?print_preview.Invitation} */
    invitation_: {
      type: Object,
      value: null,
    },

    /** @type {!print_preview.UserInfo} */
    userInfo: {
      type: Object,
      notify: true,
    },

    /** @type {boolean} */
    showCloudPrintPromo: {
      type: Boolean,
      notify: true,
      observer: 'onShowCloudPrintPromoChanged_',
    },

    /** @private {!Array<!print_preview.Destination>} */
    destinations_: {
      type: Array,
      notify: true,
      value: [],
    },

    /** @private {boolean} */
    loadingDestinations_: {
      type: Boolean,
      value: false,
    },

    /** @type {!Array<!print_preview.RecentDestination>} */
    recentDestinations: Array,

    /** @private {!Array<!print_preview.Destination>} */
    recentDestinationList_: {
      type: Array,
      notify: true,
      computed: 'computeRecentDestinationList_(' +
          'destinationStore, recentDestinations, recentDestinations.*, ' +
          'userInfo, destinations_.*)',
      observer: 'onRecentDestinationListChange_',
    },

    /** @private {?RegExp} */
    searchQuery_: {
      type: Object,
      value: null,
    },
  },

  listeners: {
    'keydown': 'onKeydown_',
  },

  /** @private {!EventTracker} */
  tracker_: new EventTracker(),

  /** @private {!print_preview.DestinationSearchMetricsContext} */
  metrics_: new print_preview.DestinationSearchMetricsContext(),

  // <if expr="chromeos">
  /** @private {?print_preview.Destination} */
  destinationInConfiguring_: null,
  // </if>

  /** @override */
  ready: function() {
    this.$$('.promo-text').innerHTML =
        this.i18nAdvanced('cloudPrintPromotion', {
          substitutions: ['<a is="action-link" class="sign-in">', '</a>'],
          attrs: {
            'is': (node, v) => v == 'action-link',
            'class': (node, v) => v == 'sign-in',
            'tabindex': (node, v) => v == '0',
            'role': (node, v) => v == 'link',
          },
        });
  },

  /** @override */
  attached: function() {
    this.tracker_.add(
        assert(this.$$('.sign-in')), 'click', this.onSignInClick_.bind(this));
  },

  /**
   * @param {!KeyboardEvent} e Event containing the key
   * @private
   */
  onKeydown_: function(e) {
    e.stopPropagation();
    const searchInput = this.$.searchBox.getSearchInput();
    if (e.key == 'Escape' &&
        (e.composedPath()[0] !== searchInput || !searchInput.value.trim())) {
      this.$.dialog.cancel();
      e.preventDefault();
    }
  },

  /** @private */
  onDestinationStoreSet_: function() {
    assert(this.destinations_.length == 0);
    const destinationStore = assert(this.destinationStore);
    this.tracker_.add(
        destinationStore,
        print_preview.DestinationStore.EventType.DESTINATIONS_INSERTED,
        this.updateDestinations_.bind(this));
    this.tracker_.add(
        destinationStore,
        print_preview.DestinationStore.EventType.DESTINATIONS_RESET,
        () => this.destinations_ = []);
    this.tracker_.add(
        destinationStore,
        print_preview.DestinationStore.EventType.DESTINATION_SEARCH_DONE,
        this.onDestinationSearchDone_.bind(this));
  },

  /** @private */
  onInvitationStoreSet_: function() {
    const invitationStore = assert(this.invitationStore);
    this.tracker_.add(
        invitationStore,
        print_preview.InvitationStore.EventType.INVITATION_SEARCH_DONE,
        this.updateInvitations_.bind(this));
    this.tracker_.add(
        invitationStore,
        print_preview.InvitationStore.EventType.INVITATION_PROCESSED,
        this.updateInvitations_.bind(this));
  },

  /** @private */
  onDestinationSearchDone_: function() {
    this.updateDestinations_();
    this.invitationStore.startLoadingInvitations();
  },

  /** @private */
  updateDestinations_: function() {
    if (this.destinationStore === undefined)
      return;

    this.notifyPath('userInfo.users');
    this.notifyPath('userInfo.activeUser');
    this.notifyPath('userInfo.loggedIn');
    if (this.userInfo.loggedIn)
      this.showCloudPrintPromo = false;

    if (this.userInfo) {
      this.updateList(
          'destinations_',
          destination => destination.origin + '/' + destination.id,
          this.destinationStore.destinations(this.userInfo.activeUser));
    } else {
      this.destinations_ = [];
    }

    this.loadingDestinations_ =
        this.destinationStore.isPrintDestinationSearchInProgress;
  },

  /**
   * @return {!Array<!print_preview.Destination>}
   * @private
   */
  computeRecentDestinationList_: function() {
    if (!observerDepsDefined(Array.from(arguments)))
      return [];

    let recentDestinations = [];
    const filterAccount = this.userInfo.activeUser;
    this.recentDestinations.forEach((recentDestination) => {
      const destination = this.destinationStore.getDestination(
          recentDestination.origin, recentDestination.id,
          recentDestination.account || '');
      if (destination &&
          (!destination.account || destination.account == filterAccount)) {
        recentDestinations.push(destination);
      }
    });
    return recentDestinations;
  },

  /** @private */
  onRecentDestinationListChange_: function() {
    const numRecent = Math.max(2, this.recentDestinationList_.length);
    this.$.recentList.style.maxHeight = `calc(${numRecent} *
            var(--destination-item-height) + 10px + 20 / 13 * 1rem)`;
  },

  /** @private */
  onCloseOrCancel_: function() {
    if (this.searchQuery_)
      this.$.searchBox.setValue('');
    if (this.$.dialog.getNative().returnValue == 'success') {
      this.metrics_.record(print_preview.Metrics.DestinationSearchBucket
                               .DESTINATION_CLOSED_CHANGED);
    } else {
      this.metrics_.record(print_preview.Metrics.DestinationSearchBucket
                               .DESTINATION_CLOSED_UNCHANGED);
    }
  },

  /** @private */
  onCancelButtonClick_: function() {
    this.$.dialog.cancel();
  },

  /**
   * @param {!CustomEvent} e Event containing the selected destination list item
   *     element.
   * @private
   */
  onDestinationSelected_: function(e) {
    const listItem =
        /** @type {!PrintPreviewDestinationListItemElement} */ (e.detail);
    const destination = listItem.destination;

    // ChromeOS local destinations that don't have capabilities need to be
    // configured before selecting, and provisional destinations need to be
    // resolved. Other destinations can be selected.
    if (destination.readyForSelection) {
      this.selectDestination_(destination);
      return;
    }

    // Provisional destinations
    if (destination.isProvisional) {
      this.$.provisionalResolver.resolveDestination(destination)
          .then(this.selectDestination_.bind(this))
          .catch(function() {
            console.warn(
                'Failed to resolve provisional destination: ' + destination.id);
          })
          .then(() => {
            if (this.$.dialog.open && !!listItem && !listItem.hidden) {
              listItem.focus();
            }
          });
      return;
    }

    // <if expr="chromeos">
    // Destination must be a CrOS local destination that needs to be set up.
    // The user is only allowed to set up printer at one time.
    if (this.destinationInConfiguring_)
      return;

    // Show the configuring status to the user and resolve the destination.
    listItem.onConfigureRequestAccepted();
    this.destinationInConfiguring_ = destination;
    this.destinationStore.resolveCrosDestination(destination)
        .then(
            response => {
              this.destinationInConfiguring_ = null;
              listItem.onConfigureComplete(response.success);
              if (response.success) {
                destination.capabilities = response.capabilities;
                if (response.policies)
                  destination.policies = response.policies;
                this.selectDestination_(destination);
              }
            },
            () => {
              this.destinationInConfiguring_ = null;
              listItem.onConfigureComplete(false);
            });
    // </if>
  },

  /**
   * @param {!print_preview.Destination} destination The destination to select.
   * @private
   */
  selectDestination_: function(destination) {
    this.destinationStore.selectDestination(destination);
    this.$.dialog.close();
  },

  show: function() {
    this.$.dialog.showModal();
    this.loadingDestinations_ = this.destinationStore === undefined ||
        this.destinationStore.isPrintDestinationSearchInProgress;
    this.metrics_.record(
        print_preview.Metrics.DestinationSearchBucket.DESTINATION_SHOWN);
    this.$.recentList.forceIronResize();
    this.$.printList.forceIronResize();
  },

  /** @return {boolean} Whether the dialog is open. */
  isOpen: function() {
    return this.$.dialog.hasAttribute('open');
  },

  /** @private */
  isSelected_: function(account) {
    return account == this.userInfo.activeUser;
  },

  /** @private */
  onSignInClick_: function() {
    this.metrics_.record(
        print_preview.Metrics.DestinationSearchBucket.SIGNIN_TRIGGERED);
    print_preview.NativeLayer.getInstance().signIn(false).then(() => {
      this.destinationStore.onDestinationsReload();
    });
  },

  /** @private */
  onCloudPrintPromoDismissed_: function() {
    this.showCloudPrintPromo = false;
  },

  /**
   * Updates printer sharing invitations UI.
   * @private
   */
  updateInvitations_: function() {
    const invitations = this.userInfo.activeUser ?
        this.invitationStore.invitations(this.userInfo.activeUser) :
        [];
    if (this.invitation_ != invitations[0]) {
      this.metrics_.record(
          print_preview.Metrics.DestinationSearchBucket.INVITATION_AVAILABLE);
    }
    this.invitation_ = invitations.length > 0 ? invitations[0] : null;
  },

  /**
   * @return {string} The text show show on the "accept" button in the
   *     invitation promo. 'Accept', 'Accept for group', or empty if there is no
   *     invitation.
   * @private
   */
  getAcceptButtonText_: function() {
    if (!this.invitation_)
      return '';

    return this.invitation_.asGroupManager ? this.i18n('acceptForGroup') :
                                             this.i18n('accept');
  },

  /**
   * @return {string} The formatted text to show for the invitation promo.
   * @private
   */
  getInvitationText_: function() {
    if (!this.invitation_)
      return '';

    if (this.invitation_.asGroupManager) {
      return this.i18nAdvanced('groupPrinterSharingInviteText', {
        substitutions: [
          this.invitation_.sender, this.invitation_.destination.displayName,
          this.invitation_.receiver
        ]
      });
    }

    return this.i18nAdvanced('printerSharingInviteText', {
      substitutions:
          [this.invitation_.sender, this.invitation_.destination.displayName]
    });
  },

  /** @private */
  onInvitationAcceptClick_: function() {
    this.metrics_.record(
        print_preview.Metrics.DestinationSearchBucket.INVITATION_ACCEPTED);
    this.invitationStore.processInvitation(assert(this.invitation_), true);
    this.updateInvitations_();
  },

  /** @private */
  onInvitationRejectClick_: function() {
    this.metrics_.record(
        print_preview.Metrics.DestinationSearchBucket.INVITATION_REJECTED);
    this.invitationStore.processInvitation(assert(this.invitation_), false);
    this.updateInvitations_();
  },

  /** @private */
  onUserChange_: function() {
    const select = this.$$('select');
    const account = select.value;
    if (account) {
      this.showCloudPrintPromo = false;
      this.userInfo.activeUser = account;
      this.notifyPath('userInfo.activeUser');
      this.notifyPath('userInfo.loggedIn');
      this.destinationStore.reloadUserCookieBasedDestinations();
      this.invitationStore.startLoadingInvitations();
      this.metrics_.record(
          print_preview.Metrics.DestinationSearchBucket.ACCOUNT_CHANGED);
    } else {
      print_preview.NativeLayer.getInstance().signIn(true).then(
          this.destinationStore.onDestinationsReload.bind(
              this.destinationStore));
      const options = select.querySelectorAll('option');
      for (let i = 0; i < options.length; i++) {
        if (options[i].value == this.userInfo.activeUser) {
          select.selectedIndex = i;
          break;
        }
      }
      this.metrics_.record(
          print_preview.Metrics.DestinationSearchBucket.ADD_ACCOUNT_SELECTED);
    }
  },

  /** @private */
  onShowCloudPrintPromoChanged_: function() {
    if (this.showCloudPrintPromo) {
      this.metrics_.record(
          print_preview.Metrics.DestinationSearchBucket.SIGNIN_PROMPT);
    }
  },
});

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link_css.m.js';
import './print_preview_shared_css.js';
import './print_preview_vars_css.js';
import './throbber_css.js';
import '../strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Invitation} from '../data/invitation.js';
import {InvitationStore} from '../data/invitation_store.js';
import {Metrics, MetricsContext} from '../metrics.js';

Polymer({
  is: 'print-preview-invitation-promo',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    activeUser: {
      type: String,
      observer: 'onActiveUserChanged_',
    },

    /** @type {?InvitationStore} */
    invitationStore: Object,

    /** @type {!MetricsContext} */
    metrics: Object,

    /** @private {?Invitation} */
    invitation_: {
      type: Object,
      value: null,
    },

    /** @private {boolean} */
    isHidden_: {
      type: Boolean,
      reflectToAttribute: true,
      computed: 'computeIsHidden_(invitation_)',
    }
  },

  /** @private {!EventTracker} */
  tracker_: new EventTracker(),

  /** @private */
  attached() {
    const invitationStore = assert(this.invitationStore);
    this.tracker_.add(
        invitationStore, InvitationStore.EventType.INVITATION_SEARCH_DONE,
        this.updateInvitations_.bind(this));
    this.tracker_.add(
        invitationStore, InvitationStore.EventType.INVITATION_PROCESSED,
        this.updateInvitations_.bind(this));
    this.updateInvitations_();
  },

  /** @override */
  detached() {
    this.tracker_.removeAll();
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsHidden_() {
    return !this.invitation_;
  },

  /** @private */
  onActiveUserChanged_() {
    if (!this.activeUser) {
      return;
    }

    this.invitationStore.startLoadingInvitations(this.activeUser);
  },

  /**
   * Updates printer sharing invitations UI.
   * @private
   */
  updateInvitations_() {
    const invitations = this.activeUser ?
        this.invitationStore.invitations(this.activeUser) :
        [];
    if (this.invitation_ !== invitations[0]) {
      this.metrics.record(Metrics.DestinationSearchBucket.INVITATION_AVAILABLE);
    }
    this.invitation_ = invitations.length > 0 ? invitations[0] : null;
  },

  /**
   * @return {string} The text show show on the "accept" button in the
   *     invitation promo. 'Accept', 'Accept for group', or empty if there is no
   *     invitation.
   * @private
   */
  getAcceptButtonText_() {
    if (!this.invitation_) {
      return '';
    }

    return this.invitation_.asGroupManager ? this.i18n('acceptForGroup') :
                                             this.i18n('accept');
  },

  /**
   * @return {string} The formatted text to show for the invitation promo.
   * @private
   */
  getInvitationText_() {
    if (!this.invitation_) {
      return '';
    }

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
  onAcceptClick_() {
    this.metrics.record(Metrics.DestinationSearchBucket.INVITATION_ACCEPTED);
    this.invitationStore.processInvitation(assert(this.invitation_), true);
    this.updateInvitations_();
  },

  /** @private */
  onRejectClick_() {
    this.metrics.record(Metrics.DestinationSearchBucket.INVITATION_REJECTED);
    this.invitationStore.processInvitation(assert(this.invitation_), false);
    this.updateInvitations_();
  },
});

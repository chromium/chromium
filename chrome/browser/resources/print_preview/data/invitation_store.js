// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';

import {CloudPrintInterface, CloudPrintInterfaceEventType, CloudPrintInterfaceInvitesDoneDetail, CloudPrintInterfaceProcessInviteDetail} from '../cloud_print_interface.js';

import {Invitation} from './invitation.js';

/**
 * @enum {number}
 * @private
 */
const InvitationStoreLoadStatus = {
  IN_PROGRESS: 1,
  DONE: 2,
  FAILED: 3,
};

export class InvitationStore extends EventTarget {
  /** Printer sharing invitations data store. */
  constructor() {
    super();

    /**
     * Maps user account to the list of invitations for this account.
     * @private {!Object<!Array<!Invitation>>}
     */
    this.invitations_ = {};

    /**
     * Maps user account to the flag whether the invitations for this account
     * were successfully loaded.
     * @private {!Object<InvitationStoreLoadStatus>}
     */
    this.loadStatus_ = {};

    /**
     * Event tracker used to track event listeners of the destination store.
     * @private {!EventTracker}
     */
    this.tracker_ = new EventTracker();

    /**
     * Used to fetch and process invitations.
     * @private {CloudPrintInterface}
     */
    this.cloudPrintInterface_ = null;

    /**
     * Invitation being processed now. Only one invitation can be processed at
     * a time.
     * @private {Invitation}
     */
    this.invitationInProgress_ = null;
  }

  /**
   * @return {Invitation} Currently processed invitation or
   *     {@code null}.
   */
  get invitationInProgress() {
    return this.invitationInProgress_;
  }

  /**
   * @param {string} account Account to filter invitations by.
   * @return {!Array<!Invitation>} List of invitations for the
   *     {@code account}.
   */
  invitations(account) {
    return this.invitations_[account] || [];
  }

  /**
   * Sets the invitation store's Google Cloud Print interface.
   * @param {!CloudPrintInterface} cloudPrintInterface Interface
   *     to set.
   */
  setCloudPrintInterface(cloudPrintInterface) {
    assert(this.cloudPrintInterface_ == null);
    this.cloudPrintInterface_ = cloudPrintInterface;
    this.tracker_.add(
        this.cloudPrintInterface_.getEventTarget(),
        CloudPrintInterfaceEventType.INVITES_DONE,
        this.onCloudPrintInvitesDone_.bind(this));
    this.tracker_.add(
        this.cloudPrintInterface_.getEventTarget(),
        CloudPrintInterfaceEventType.INVITES_FAILED,
        this.onCloudPrintInvitesFailed_.bind(this));
    this.tracker_.add(
        this.cloudPrintInterface_.getEventTarget(),
        CloudPrintInterfaceEventType.PROCESS_INVITE_DONE,
        this.onCloudPrintProcessInviteDone_.bind(this));
  }

  /** Removes all events being tracked from the tracker. */
  resetTracker() {
    this.tracker_.removeAll();
  }

  /**
   * Initiates loading of cloud printer sharing invitations for the user
   * account given by |user|.
   * @param {string} user The user to load invitations for.
   */
  startLoadingInvitations(user) {
    if (!this.cloudPrintInterface_) {
      return;
    }
    if (this.loadStatus_.hasOwnProperty(user)) {
      if (this.loadStatus_[user] == InvitationStoreLoadStatus.DONE) {
        this.dispatchEvent(
            new CustomEvent(InvitationStore.EventType.INVITATION_SEARCH_DONE));
      }
      return;
    }

    this.loadStatus_[user] = InvitationStoreLoadStatus.IN_PROGRESS;
    this.cloudPrintInterface_.invites(user);
  }

  /**
   * Accepts or rejects the {@code invitation}, based on {@code accept} value.
   * @param {!Invitation} invitation Invitation to process.
   * @param {boolean} accept Whether to accept this invitation.
   */
  processInvitation(invitation, accept) {
    if (this.invitationInProgress_) {
      return;
    }
    this.invitationInProgress_ = invitation;
    this.cloudPrintInterface_.processInvite(invitation, accept);
  }

  /**
   * Removes processed invitation from the internal storage.
   * @param {!Invitation} invitation Processed invitation.
   * @private
   */
  invitationProcessed_(invitation) {
    if (this.invitations_.hasOwnProperty(invitation.account)) {
      this.invitations_[invitation.account] =
          this.invitations_[invitation.account].filter(function(i) {
            return i != invitation;
          });
    }
    if (this.invitationInProgress_ == invitation) {
      this.invitationInProgress_ = null;
    }
  }

  /**
   * Called when printer sharing invitations are fetched.
   * @param {!CustomEvent<!CloudPrintInterfaceInvitesDoneDetail>}
   *     event Contains the list of invitations.
   * @private
   */
  onCloudPrintInvitesDone_(event) {
    this.loadStatus_[event.detail.user] = InvitationStoreLoadStatus.DONE;
    this.invitations_[event.detail.user] = event.detail.invitations;

    this.dispatchEvent(
        new CustomEvent(InvitationStore.EventType.INVITATION_SEARCH_DONE));
  }

  /**
   * Called when printer sharing invitations fetch has failed.
   * @param {!CustomEvent<string>} event Contains the user for whom invite
   *     fetch failed.
   * @private
   */
  onCloudPrintInvitesFailed_(event) {
    this.loadStatus_[event.detail] = InvitationStoreLoadStatus.FAILED;
  }

  /**
   * Called when printer sharing invitation was processed successfully.
   * @param {!CustomEvent<!CloudPrintInterfaceProcessInviteDetail>}
   *     event Contains detailed information about the invite.
   * @private
   */
  onCloudPrintProcessInviteDone_(event) {
    this.invitationProcessed_(event.detail.invitation);
    this.dispatchEvent(
        new CustomEvent(InvitationStore.EventType.INVITATION_PROCESSED));
  }
}

/**
 * Event types dispatched by the data store.
 * @enum {string}
 */
InvitationStore.EventType = {
  INVITATION_PROCESSED: 'InvitationStore.INVITATION_PROCESSED',
  INVITATION_SEARCH_DONE: 'InvitationStore.INVITATION_SEARCH_DONE'
};

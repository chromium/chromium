// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'discards-tab',

  behaviors: [SortedTableBehavior],

  properties: {
    /**
     * List of tabinfos.
     * @private {?Array<!mojom.TabDiscardsInfo>}
     */
    tabInfos_: {
      type: Array,
    },
  },

  /** @private The current update timer if any. */
  updateTimer_: 0,

  /** @private {(mojom.DiscardsDetailsProviderPtr|null)} */
  uiHandler_: null,

  /** @override */
  ready: function() {
    this.setSortKey('utilityRank');
    this.uiHandler_ = discards.getOrCreateUiHandler();

    this.updateTable_();
  },

  /**
   * Returns a sort function to compare tab infos based on the provided sort key
   * and a boolean reverse flag.
   * @param {string} sortKey The sort key for the  returned function.
   * @param {boolean} sortReverse True if sorting is reversed.
   * @return {function({Object}, {Object}): number}
   *     A comparison function that compares two tab infos, returns
   *     negative number if a < b, 0 if a == b, and a positive
   *     number if a > b.
   * @private
   */
  computeSortFunction_: function(sortKey, sortReverse) {
    // Polymer 2.0 may invoke multi-property observers before all properties
    // are defined.
    if (!sortKey)
      return (a, b) => 0;

    return function(a, b) {
      const comp = discards.compareTabDiscardsInfos(sortKey, a, b);
      return sortReverse ? -comp : comp;
    };
  },

  /**
   * Returns a string representation of a visibility enum value for display in
   * a table.
   * @param {mojom.LifecycleUnitVisibility} visibility A visibility value.
   * @return {string} A string representation of the visibility.
   * @private
   */
  visibilityToString_: function(visibility) {
    switch (visibility) {
      case mojom.LifecycleUnitVisibility.HIDDEN:
        return 'hidden';
      case mojom.LifecycleUnitVisibility.OCCLUDED:
        return 'occluded';
      case mojom.LifecycleUnitVisibility.VISIBLE:
        return 'visible';
    }
    assertNotReached('Unknown visibility: ' + visibility);
  },

  /**
   * Returns a string representation of a loading state enum value for display
   * in a table.
   * @param {mojom.LifecycleUnitLoadingState} loadingState A loading state
   *    value.
   * @return {string} A string representation of the loading state.
   * @private
   */
  loadingStateToString_: function(loadingState) {
    switch (loadingState) {
      case mojom.LifecycleUnitLoadingState.UNLOADED:
        return 'unloaded';
      case mojom.LifecycleUnitLoadingState.LOADING:
        return 'loading';
      case mojom.LifecycleUnitLoadingState.LOADED:
        return 'loaded';
    }
    assertNotReached('Unknown loadingState: ' + loadingState);
  },

  /**
   * Returns a string representation of a discard reason.
   * @param {mojom.LifecycleUnitDiscardReason} reason The discard reason.
   * @return {string} A string representation of the discarding reason.
   * @private
   */
  discardReasonToString_: function(reason) {
    switch (reason) {
      case mojom.LifecycleUnitDiscardReason.EXTERNAL:
        return 'external';
      case mojom.LifecycleUnitDiscardReason.PROACTIVE:
        return 'proactive';
      case mojom.LifecycleUnitDiscardReason.URGENT:
        return 'urgent';
    }
    assertNotReached('Unknown discard reason: ' + reason);
  },

  /**
   * Returns a string representation of a lifecycle state.
   * @param {mojom.LifecycleUnitState} state The lifecycle state.
   * @param {mojom.LifecycleUnitDiscardReason} reason The discard reason. This
   *     is only used if the state is discard related.
   * @param {mojom.LifecycleUnitVisibility} visibility A visibility value.
   * @param {boolean} hasFocus Whether or not the tab has input focus.
   * @return {string} A string representation of the lifecycle state, augmented
   *     with the discard reason if appropriate.
   * @private
   */
  lifecycleStateToString_: function(state, reason, visibility, hasFocus) {
    const pageLifecycleStateFromVisibilityAndFocus = function() {
      switch (visibility) {
        case mojom.LifecycleUnitVisibility.HIDDEN:
        case mojom.LifecycleUnitVisibility.OCCLUDED:
          // An occluded page is also considered hidden.
          return 'hidden';
        case mojom.LifecycleUnitVisibility.VISIBLE:
          return hasFocus ? 'active' : 'passive';
      }
      assertNotReached('Unknown visibility: ' + visibility);
    };

    switch (state) {
      case mojom.LifecycleUnitState.ACTIVE:
        return pageLifecycleStateFromVisibilityAndFocus();
      case mojom.LifecycleUnitState.THROTTLED:
        return pageLifecycleStateFromVisibilityAndFocus() + ' (throttled)';
      case mojom.LifecycleUnitState.PENDING_FREEZE:
        return pageLifecycleStateFromVisibilityAndFocus() + ' (pending frozen)';
      case mojom.LifecycleUnitState.FROZEN:
        return 'frozen';
      case mojom.LifecycleUnitState.PENDING_DISCARD:
        return pageLifecycleStateFromVisibilityAndFocus() +
            ' (pending discard (' + this.discardReasonToString_(reason) + '))';
      case mojom.LifecycleUnitState.DISCARDED:
        return 'discarded (' + this.discardReasonToString_(reason) + ')';
      case mojom.LifecycleUnitState.PENDING_UNFREEZE:
        return 'frozen (pending unfreeze)';
    }
    assertNotReached('Unknown lifecycle state: ' + state);
  },

  /**
   * Dispatches a request to update tabInfos_.
   * @private
   */
  updateTableImpl_: function() {
    this.uiHandler_.getTabDiscardsInfo().then(response => {
      this.tabInfos_ = response.infos;
    });
  },

  /**
   * A wrapper to updateTableImpl_ that is called due to user action and not
   * due to the automatic timer. Cancels the existing timer  and reschedules it
   * after rendering instantaneously.
   * @private
   */
  updateTable_: function() {
    if (this.updateTimer_)
      clearInterval(this.updateTimer_);
    this.updateTableImpl_();
    this.updateTimer_ = setInterval(this.updateTableImpl_.bind(this), 1000);
  },

  /**
   * Formats an items reactivation for display.
   * @param {mojom.TabDiscardsInfo} item The item in question.
   * @return {string} The formatted reactivation score.
   * @private
   */
  getReactivationScore_: function(item) {
    return item.hasReactivationScore ? item.reactivationScore.toFixed(4) :
                                       'N/A';
  },

  /**
   * Formats an items site engagement score for display.
   * @param {mojom.TabDiscardsInfo} item The item in question.
   * @return {string} The formatted site engagemetn score.
   * @private
   */
  getSiteEngagementScore_: function(item) {
    return item.siteEngagementScore.toFixed(1);
  },

  /**
   * Retrieves favicon style tag value for an item.
   * @param {mojom.TabDiscardsInfo} item The item in question.
   * @return {string} A style to retrieve and display the item's favicon.
   * @private
   */
  getFavIconStyle_: function(item) {
    return 'background-image:' + cr.icon.getFavicon(item.tabUrl);
  },

  /**
   * Formats an items lifecycle state for display.
   * @param {mojom.TabDiscardsInfo} item The item in question.
   * @return {string} A human readable lifecycle state.
   * @private
   */
  getLifeCycleState_: function(item) {
    if (item.loadingState != mojom.LifecycleUnitLoadingState.UNLOADED ||
        item.discardCount > 0) {
      return this.lifecycleStateToString_(
          item.state, item.discardReason, item.visibility, item.hasFocus);
    } else {
      return '';
    }
  },

  /**
   * Returns a string representation of a boolean value for display in a table.
   * @param {boolean} value A boolean value.
   * @return {string} A string representing the bool.
   * @private
   */
  boolToString_: function(value) {
    return discards.boolToString(value);
  },

  /**
   * Converts a |secondsAgo| duration to a user friendly string.
   * @param {number} secondsAgo The duration to render.
   * @return {string} An English string representing the duration.
   * @private
   */
  durationToString_: function(secondsAgo) {
    return discards.durationToString(secondsAgo);
  },

  /**
   * Tests whether an item has reasons why it cannot be frozen.
   * @param {mojom.TabDiscardsInfo} item The item in question.
   * @return {boolean} true iff there are reasons why the item cannot be frozen.
   * @private
   */
  hasCannotFreezeReasons_: function(item) {
    return item.cannotFreezeReasons.length != 0;
  },

  /**
   * Returns a string with the reasons an item cannot be frozen, for display in
   * a tooltip.
   * @param {mojom.TabDiscardsInfo} item The item in question.
   * @return {string} A string with the reasons.
   * @private
   */
  cannotFreezeReasons_: function(item) {
    return item.cannotFreezeReasons.join('<br />');
  },

  /**
   * Tests whether an item has reasons why it cannot be discarded.
   * @param {mojom.TabDiscardsInfo} item The item in question.
   * @return {boolean} true iff there are reasons why the item cannot be
   *     discarded.
   * @private
   */
  hasCannotDiscardReasons_: function(item) {
    return item.cannotDiscardReasons.length != 0;
  },

  /**
   * Returns a string with the reasons an item cannot be discarded, for display
   * in a tooltip.
   * @param {mojom.TabDiscardsInfo} item The item in question.
   * @return {string} A string with the reasons.
   * @private
   */
  cannotDiscardReasons_: function(item) {
    return item.cannotDiscardReasons.join('<br />');
  },

  /**
   * Tests whether an item can be loaded.
   * @param {mojom.TabDiscardsInfo} item The item in question.
   * @return {boolean} true iff the item can be loaded.
   * @private
   */
  canLoad_: function(item) {
    return item.loadingState == mojom.LifecycleUnitLoadingState.UNLOADED;
  },

  /**
   * Tests whether an item can be frozen.
   * @param {mojom.TabDiscardsInfo} item The item in question.
   * @return {boolean} true iff the item can be frozen.
   * @private
   */
  canFreeze_: function(item) {
    if (item.visibility == mojom.LifecycleUnitVisibility.HIDDEN ||
        item.visibility == mojom.LifecycleUnitVisibility.OCCLUDED) {
      // Only tabs that aren't visible can be frozen for now.
      switch (item.state) {
        case mojom.LifecycleUnitState.DISCARDED:
        case mojom.LifecycleUnitState.PENDING_DISCARD:
        case mojom.LifecycleUnitState.FROZEN:
        case mojom.LifecycleUnitState.PENDING_FREEZE:
          return false;
      }
      return true;
    }
    return false;
  },

  /**
   * Tests whether an item can be discarded.
   * @param {mojom.TabDiscardsInfo} item The item in question.
   * @return {boolean} true iff the item can be discarded.
   * @private
   */
  canDiscard_: function(item) {
    if (item.visibility == mojom.LifecycleUnitVisibility.HIDDEN ||
        item.visibility == mojom.LifecycleUnitVisibility.OCCLUDED) {
      // Only tabs that aren't visible can be discarded for now.
      switch (item.state) {
        case mojom.LifecycleUnitState.DISCARDED:
        case mojom.LifecycleUnitState.PENDING_DISCARD:
          return false;
      }
      return true;
    }
    return false;
  },

  /**
   * Event handler that toggles the auto discardable flag on an item.
   * @param {Event} e The event.
   * @private
   */
  toggleAutoDiscardable_: function(e) {
    const item = e.model.item;
    this.uiHandler_.setAutoDiscardable(item.id, !item.isAutoDiscardable)
        .then(this.updateTable_.bind(this));
  },

  /**
   * Event handler that loads a tab.
   * @param {Event} e The event.
   * @private
   */
  loadTab_: function(e) {
    this.uiHandler_.loadById(e.model.item.id);
  },

  /**
   * Event handler that freezes a tab.
   * @param {Event} e The event.
   * @private
   */
  freezeTab_: function(e) {
    this.uiHandler_.freezeById(e.model.item.id);
  },

  /**
   * Implementation function for tab discarding.
   * @param {Event} e The event.
   * @param {boolean} urgent True if tab should be urgently discarded.
   * @private
   */
  discardTabImpl_: function(e, urgent) {
    this.uiHandler_.discardById(e.model.item.id, urgent)
        .then(this.updateTable_.bind(this));
  },

  /**
   * Event handler that discards a given tab.
   * @param {Event} e The event.
   * @private
   */
  discardTab_: function(e) {
    this.discardTabImpl_(e, false);
  },

  /**
   * Event handler that discards a given tab urgently.
   * @param {Event} e The event.
   * @private
   */
  urgentDiscardTab_: function(e) {
    this.discardTabImpl_(e, true);
  },

  /**
   * Implementation function to discard the next discardable tab.
   * @param {boolean} urgent True if tab should be urgently discarded.
   * @private
   */
  discardImpl_: function(urgent) {
    this.uiHandler_.discard(urgent).then(() => {
      this.updateTable_();
    });
  },

  /**
   * Event handler that discards the next discardable tab.
   * @param {Event} e The event.
   * @private
   */
  discardNow_: function(e) {
    this.discardImpl_(false);
  },

  /**
   * Event handler that discards the next discardable tab urgently.
   * @param {Event} e The event.
   * @private
   */
  discardUrgentNow_: function(e) {
    this.discardImpl_(true);
  },
});

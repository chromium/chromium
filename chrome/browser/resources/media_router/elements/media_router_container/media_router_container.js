// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This Polymer element contains the entire media router interface. It handles
 * hiding and showing specific components.
 * @implements {MediaRouterContainerInterface}
 */
Polymer({
  is: 'media-router-container',

  properties: {
    /**
     * The list of available sinks.
     * @type {!Array<!media_router.Sink>}
     */
    allSinks: {
      type: Array,
      value: [],
      observer: 'reindexSinksAndRebuildSinksToShow_',
    },

    /**
     * The last promise in a chain that will be fulfilled when the current
     * animation has finished. It does not return a value; it is strictly a
     * synchronization mechanism.
     * @private {!Promise}
     */
    animationPromise_: {
      type: Object,
      value: function() {
        return Promise.resolve();
      },
    },

    /**
     * The list of CastModes to show.
     * @type {!Array<!media_router.CastMode>|undefined}
     */
    castModeList: {
      type: Array,
      observer: 'checkCurrentCastMode_',
    },

    /**
     * The ID of the Sink currently being launched.
     * @private {string}
     * TODO(crbug.com/616604): Use per-sink route creation state.
     */
    currentLaunchingSinkId_: {
      type: String,
      value: '',
    },

    /**
     * The current route.
     * @private {?media_router.Route|undefined}
     */
    currentRoute_: {
      type: Object,
    },

    /**
     * The current view to be shown.
     * @private {?media_router.MediaRouterView|undefined}
     */
    currentView_: {
      type: String,
      observer: 'currentViewChanged_',
    },

    /**
     * The URL to open when the device missing link is clicked.
     * @type {string|undefined}
     */
    deviceMissingUrl: {
      type: String,
    },

    /**
     * The height of the dialog.
     * @private {number}
     */
    dialogHeight_: {
      type: Number,
      value: 330,
    },

    /**
     * The time |this| element calls ready().
     * @private {number|undefined}
     */
    elementReadyTimeMs_: {
      type: Number,
    },

    /**
     * Animation player used for running filter transition animations.
     * @private {?Animation}
     */
    filterTransitionPlayer_: {
      type: Object,
      value: null,
    },

    /**
     * The URL to open when the cloud services pref learn more link is clicked.
     * @type {string|undefined}
     */
    firstRunFlowCloudPrefLearnMoreUrl: {
      type: String,
    },

    /**
     * The URL to open when the first run flow learn more link is clicked.
     * @type {string|undefined}
     */
    firstRunFlowLearnMoreUrl: {
      type: String,
    },

    /**
     * The header text for the sink list.
     * @type {string|undefined}
     */
    headerText: {
      type: String,
    },

    /**
     * The header text tooltip. This would be descriptive of the
     * source origin, whether a host name, tab URL, etc.
     * @type {string|undefined}
     */
    headerTextTooltip: {
      type: String,
    },

    /**
     * An animation player that is used for running dialog height adjustments.
     * @private {?Animation}
     */
    heightAdjustmentPlayer_: {
      type: Object,
      value: null,
    },

    /**
     * Whether the sink list is being hidden for animation purposes.
     * @private {boolean}
     */
    hideSinkListForAnimation_: {
      type: Boolean,
      value: false,
    },

    /**
     * Records whether the search input is focused when a window blur event is
     * received. This is used to handle search focus edge cases. See
     * |setSearchFocusHandlers_| for details.
     * @private {boolean}
     */
    isSearchFocusedOnWindowBlur_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the search list is currently hidden.
     * @private {boolean}
     */
    isSearchListHidden_: {
      type: Boolean,
      value: true,
    },

    /**
     * The issue to show.
     * @type {?media_router.Issue}
     */
    issue: {
      type: Object,
      value: null,
      observer: 'maybeShowIssueView_',
    },

    /**
     * Whether the MR UI was just opened.
     * @private {boolean}
     */
    justOpened_: {
      type: Boolean,
      value: true,
    },

    /**
     * Whether the user's mouse is positioned over the dialog.
     * @private {boolean|undefined}
     */
    mouseIsPositionedOverDialog_: {
      type: Boolean,
    },

    /**
     * The ID of the route that is currently being created. This is set when
     * route creation is resolved but not ready for its controls to be
     * displayed.
     * @private {string|undefined}
     */
    pendingCreatedRouteId_: {
      type: String,
    },

    /**
     * The time the sink list was shown and populated with at least one sink.
     * This is reset whenever the user switches views or there are no sinks
     * available for display.
     * @private {number}
     */
    populatedSinkListSeenTimeMs_: {
      type: Number,
      value: -1,
    },

    /**
     * Pseudo sinks from MRPs that represent their ability to accept sink search
     * requests.
     * @private {!Array<!media_router.Sink>}
     */
    pseudoSinks_: {
      type: Array,
      value: [],
    },

    /**
     * Helps manage the state of creating a sink and a route from a pseudo sink.
     * @private {PseudoSinkSearchState|undefined}
     */
    pseudoSinkSearchState_: {
      type: Object,
    },

    /**
     * Whether the next character input should cause a filter action metric to
     * be sent.
     * @type {boolean}
     * @private
     */
    reportFilterOnInput_: {
      type: Boolean,
      value: false,
    },

    /**
     * The list of current routes.
     * @type {!Array<!media_router.Route>|undefined}
     */
    routeList: {
      type: Array,
      observer: 'rebuildRouteMaps_',
    },

    /**
     * Maps media_router.Route.id to corresponding media_router.Route.
     * @private {!Object<!string, !media_router.Route>|undefined}
     */
    routeMap_: {
      type: Object,
    },

    /**
     * Whether the search feature is enabled and we should show the search
     * input.
     * @private {boolean}
     */
    searchEnabled_: {
      type: Boolean,
      value: false,
      observer: 'searchEnabledChanged_',
    },

    /**
     * Search text entered by the user into the sink search input.
     * @private {string}
     */
    searchInputText_: {
      type: String,
      value: '',
      observer: 'searchInputTextChanged_',
    },

    /**
     * Sinks to display that match |searchInputText_|.
     * @private {!Array<!{sinkItem: !media_router.Sink,
     *                    substrings: Array<!Array<number>>}>|undefined}
     */
    searchResultsToShow_: {
      type: Array,
    },

    /**
     * The selected cast mode menu item. The item with this index is bolded in
     * the cast mode menu.
     * @private {number|undefined}
     */
    selectedCastModeMenuItem_: {
      type: Number,
      observer: 'updateSelectedCastModeMenuItem_',
    },

    /**
     * Whether to show the user domain of sinks associated with identity.
     * @type {boolean|undefined}
     */
    showDomain: {
      type: Boolean,
    },

    /**
     * Whether to show the first run flow.
     * @type {boolean|undefined}
     */
    showFirstRunFlow: {
      type: Boolean,
      observer: 'updateElementPositioning_',
    },

    /**
     * Whether to show the cloud preference setting in the first run flow.
     * @type {boolean|undefined}
     */
    showFirstRunFlowCloudPref: {
      type: Boolean,
    },

    /**
     * The cast mode shown to the user. Initially populated within
     * |rebuildSinksToShow_()|.
     * This value may be changed in one of the following ways:
     * 1) The user explicitly selected a cast mode.
     * 2) The user selected cast mode is no longer available for the associated
     *    WebContents. In this case, the container will reset to auto mode. Note
     *    that |userHasSelectedCastMode_| will switch back to false.
     * 3) The sink list changed, and the user had not explicitly selected a cast
     *    mode. If the sinks support exactly 1 cast mode, the container will
     *    switch to that cast mode. Otherwise, the container will reset to auto
     *    mode.
     * @private {number}
     */
    shownCastModeValue_: Number,

    /**
     * Max height for the sink list.
     * @private {number}
     */
    sinkListMaxHeight_: {
      type: Number,
      value: 0,
    },

    /**
     * Maps media_router.Sink.id to corresponding media_router.Sink.
     * @private {!Object<!string, !media_router.Sink>|undefined}
     */
    sinkMap_: {
      type: Object,
    },

    /**
     * Maps media_router.Sink.id to corresponding media_router.Route.
     * @private {!Object<!string, !media_router.Route>}
     */
    sinkToRouteMap_: {
      type: Object,
      value: {},
    },

    /**
     * Sinks to show for the currently selected cast mode.
     * @private {!Array<!media_router.Sink>|undefined}
     */
    sinksToShow_: {
      type: Array,
      observer: 'updateElementPositioning_',
    },

    /**
     * Whether the user has explicitly selected a cast mode.
     * @private {boolean}
     */
    userHasSelectedCastMode_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the user has already taken an action.
     * @type {boolean}
     */
    userHasTakenInitialAction_: {
      type: Boolean,
      value: false,
    },
  },

  behaviors: [
    I18nBehavior,
  ],

  observers: [
    'maybeUpdateStartSinkDisplayStartTime_(currentView_, sinksToShow_)',
  ],

  ready: function() {
    this.elementReadyTimeMs_ = window.performance.now();
    this.showSinkList_();

    Polymer.RenderStatus.afterNextRender(this, function() {
      // Import the elements that aren't needed at startup. This reduces
      // initial load time. Delayed loading interferes with getting the
      // offsetHeight of the first-run-flow element in updateElementPositioning_
      // though, so we also make sure it is called after the last load.
      var that = this;
      var loadsRemaining = 3;
      var onload = function() {
        loadsRemaining--;
        if (loadsRemaining > 0) {
          return;
        }
        that.updateElementPositioning_();
        if (that.currentView_ == media_router.MediaRouterView.SINK_LIST) {
          that.putSearchAtBottom_();
        }
      };
      this.importHref(
          'chrome://resources/polymer/v1_0/neon-animation/' +
              'web-animations.html',
          onload);
      this.importHref(
          this.resolveUrl('../issue_banner/issue_banner.html'), onload);
      this.importHref(
          this.resolveUrl(
              '../media_router_search_highlighter/' +
              'media_router_search_highlighter.html'),
          onload);

      // If this is not on a Mac platform, remove the placeholder. See
      // onFocus_() for more details. ready() is only called once, so no need
      // to check if the placeholder exist before removing.
      if (!cr.isMac)
        this.$$('#focus-placeholder').remove();

      document.addEventListener('keydown', this.onKeydown_.bind(this), true);
      this.listen(this, 'focus', 'onFocus_');
      this.listen(this, 'header-height-changed', 'updateElementPositioning_');
      this.listen(this, 'header-or-arrow-click', 'toggleCastModeHidden_');
      this.listen(this, 'mouseleave', 'onMouseLeave_');
      this.listen(this, 'mouseenter', 'onMouseEnter_');

      // Turn off the spinner after 3 seconds, then report the current number of
      // sinks.
      this.async(function() {
        this.justOpened_ = false;
        // |pseudoSinks_| does not contain pseudo sinks without a domain, so it
        // cannot be used for calculating the number of real sinks.
        var realSinks = this.allSinks.filter(function(sink) {
          return !sink.isPseudoSink;
        });
        this.fire('report-sink-count', {
          sinkCount: realSinks.length,
        });
      }, 3000 /* 3 seconds */);

      // For Mac platforms, request data after a short delay after load. This
      // appears to speed up initial data load time on Mac.
      if (cr.isMac) {
        this.async(function() {
          this.fire('request-initial-data');
        }, 25 /* 0.025 seconds */);
      }
    });
  },

  /**
   * Fires an acknowledge-first-run-flow event and hides the first run flow.
   * This is call when the first run flow button is clicked.
   *
   * @private
   */
  acknowledgeFirstRunFlow_: function() {
    // Only set |userOptedIntoCloudServices| if the user was shown the cloud
    // services preferences option.
    var userOptedIntoCloudServices = this.showFirstRunFlowCloudPref ?
        this.$$('#first-run-cloud-checkbox').checked :
        undefined;
    this.fire('acknowledge-first-run-flow', {
      optedIntoCloudServices: userOptedIntoCloudServices,
    });

    this.showFirstRunFlow = false;
    this.showFirstRunFlowCloudPref = false;
  },

  /**
   * Fires a 'report-initial-action' event when the user takes their first
   * action after the dialog opens. Also fires a 'report-initial-action-close'
   * event if that initial action is to close the dialog.
   * @param {!media_router.MediaRouterUserAction} initialAction
   */
  maybeReportUserFirstAction: function(initialAction) {
    if (this.userHasTakenInitialAction_)
      return;

    this.fire('report-initial-action', {
      action: initialAction,
    });

    if (initialAction == media_router.MediaRouterUserAction.CLOSE) {
      var timeToClose = window.performance.now() - this.elementReadyTimeMs_;
      this.fire('report-initial-action-close', {
        timeMs: timeToClose,
      });
    }

    this.userHasTakenInitialAction_ = true;
  },

  get header() {
    return this.$['container-header'];
  },

  /**
   * Calls all the functions to set the UI to a given cast mode.
   * @param {!media_router.CastMode} castMode The cast mode to set things to.
   * @private
   */
  castModeSelected_(castMode) {
    this.selectCastMode(castMode.type);
    this.fire('cast-mode-selected', {castModeType: castMode.type});
    this.showSinkList_();
    this.maybeReportUserFirstAction(
        media_router.MediaRouterUserAction.CHANGE_MODE);
  },

  /**
   * Checks that the currently selected cast mode is still in the
   * updated list of available cast modes. If not, then update the selected
   * cast mode to the first available cast mode on the list.
   */
  checkCurrentCastMode_: function() {
    if (!this.castModeList.length)
      return;

    // If there is a forced mode make sure it is shown.
    if (this.findForcedCastMode_()) {
      this.rebuildSinksToShow_();
    }

    // If we are currently showing auto mode, then nothing needs to be done.
    // Otherwise, if the cast mode currently shown no longer exists (regardless
    // of whether it was selected by user), then switch back to auto cast mode.
    if (this.shownCastModeValue_ != media_router.CastModeType.AUTO &&
        !this.findCastModeByType_(this.shownCastModeValue_)) {
      this.setShownCastMode_(media_router.AUTO_CAST_MODE);
      this.rebuildSinksToShow_();
    }
  },

  /**
   * Compares two search match objects for sorting. Earlier and longer matches
   * are prioritized.
   *
   * @param {!{sinkItem: !media_router.Sink,
   *           substrings: Array<!Array<number>>}} resultA
   * Parameters in |resultA|:
   *   sinkItem - sink object.
   *   substrings - start-end index pairs of substring matches.
   * @param {!{sinkItem: !media_router.Sink,
   *           substrings: Array<!Array<number>>}} resultB
   * Parameters in |resultB|:
   *   sinkItem - sink object.
   *   substrings - start-end index pairs of substring matches.
   * @return {number} -1 if |resultA| should come before |resultB|, 1 if
   *     |resultB| should come before |resultA|, and 0 if they are considered
   *     equal.
   */
  compareSearchMatches_: function(resultA, resultB) {
    var substringsA = resultA.substrings;
    var substringsB = resultB.substrings;
    var numberSubstringsA = substringsA.length;
    var numberSubstringsB = substringsB.length;

    if (numberSubstringsA == 0 && numberSubstringsB == 0) {
      return 0;
    } else if (numberSubstringsA == 0) {
      return 1;
    } else if (numberSubstringsB == 0) {
      return -1;
    }

    var loopMax = Math.min(numberSubstringsA, numberSubstringsB);
    for (var i = 0; i < loopMax; ++i) {
      var [matchStartA, matchEndA] = substringsA[i];
      var [matchStartB, matchEndB] = substringsB[i];

      if (matchStartA < matchStartB) {
        return -1;
      } else if (matchStartA > matchStartB) {
        return 1;
      }

      if (matchEndA > matchEndB) {
        return -1;
      } else if (matchEndA < matchEndB) {
        return 1;
      }
    }

    if (numberSubstringsA > numberSubstringsB) {
      return -1;
    } else if (numberSubstringsA < numberSubstringsB) {
      return 1;
    }
    return 0;
  },

  /**
   * Returns a duration in ms from a distance in pixels using a default speed of
   * 1000 pixels per second.
   * @param {number} distance Number of pixels that will be traveled.
   * @private
   */
  computeAnimationDuration_: function(distance) {
    // The duration of the animation can be found by abs(distance)/speed, where
    // speed is fixed at 1000 pixels per second, or 1 pixel per millisecond.
    return Math.abs(distance);
  },

  /**
   * If there is a forced cast mode, returns that cast mode.  If |allSinks|
   * supports only a single cast mode, returns that cast mode.  Otherwise,
   * returns AUTO_MODE. Only called if |userHasSelectedCastMode_| is |false|.
   *
   * @return {!media_router.CastMode} The single cast mode supported by
   *                                  |allSinks|, or AUTO_MODE.
   */
  computeCastMode_: function() {
    /** @const */ var forcedMode = this.findForcedCastMode_();
    if (forcedMode)
      return forcedMode;

    var allCastModes = this.allSinks.reduce(function(castModesSoFar, sink) {
      // Ignore pseudo sinks in the cast mode computation.
      return castModesSoFar | (sink.isPseudoSink ? 0 : sink.castModes);
    }, 0);

    // This checks whether |castModes| does not consist of exactly 1 cast mode.
    if (!allCastModes || allCastModes & (allCastModes - 1))
      return media_router.AUTO_CAST_MODE;

    var castMode = this.findCastModeByType_(allCastModes);
    if (castMode)
      return castMode;

    console.error('Cast mode ' + allCastModes + ' not in castModeList');
    return media_router.AUTO_CAST_MODE;
  },

  /**
   * @param {?media_router.MediaRouterView} view The current view.
   * @return {boolean} Whether or not to hide the cast mode list.
   * @private
   */
  computeCastModeListHidden_: function(view) {
    return view != media_router.MediaRouterView.CAST_MODE_LIST;
  },

  /**
   * @param {!media_router.CastMode} castMode The cast mode to determine an
   *     icon for.
   * @return {string} The icon to use.
   * @private
   */
  computeCastModeIcon_: function(castMode) {
    switch (castMode.type) {
      case media_router.CastModeType.PRESENTATION:
        return 'media-router:web';
      case media_router.CastModeType.TAB_MIRROR:
        return 'media-router:tab';
      case media_router.CastModeType.DESKTOP_MIRROR:
        return 'media-router:laptop';
      case media_router.CastModeType.LOCAL_FILE:
        return 'media-router:folder';
      default:
        return '';
    }
  },

  /**
   * @param {!Array<!media_router.CastMode>} castModeList The current list of
   *     cast modes.
   * @return {!Array<!media_router.CastMode>} The list of PRESENTATION cast
   *     modes.
   * @private
   */
  computePresentationCastModeList_: function(castModeList) {
    return castModeList.filter(function(mode) {
      return mode.type == media_router.CastModeType.PRESENTATION;
    });
  },

  /**
   * @param {!Array<!media_router.Sink>} sinksToShow The list of sinks.
   * @return {boolean} Whether or not to hide the 'devices missing' message.
   * @private
   */
  computeDeviceMissingHidden_: function(sinksToShow) {
    return sinksToShow.length != 0;
  },

  /**
   * @param {?Element} element Element to compute padding for.
   * @return {number} Computes the amount of vertical padding (top + bottom) on
   *     |element|.
   * @private
   */
  computeElementVerticalPadding_: function(element) {
    var paddingBottom, paddingTop;
    [paddingBottom, paddingTop] = this.getElementVerticalPadding_(element);
    return paddingBottom + paddingTop;
  },

  /**
   * @param {?media_router.MediaRouterView} view The current view.
   * @param {?media_router.Issue} issue The current issue.
   * @return {boolean} Whether or not to hide the header.
   * @private
   */
  computeHeaderHidden_: function(view, issue) {
    return view == media_router.MediaRouterView.ROUTE_DETAILS ||
        (view == media_router.MediaRouterView.SINK_LIST && !!issue &&
         issue.isBlocking);
  },

  /**
   * @param {?media_router.MediaRouterView} view The current view.
   * @param {string} headerText The header text for the sink list.
   * @return {string|undefined} The text for the header.
   * @private
   */
  computeHeaderText_: function(view, headerText) {
    switch (view) {
      case media_router.MediaRouterView.CAST_MODE_LIST:
        return this.i18n('selectCastModeHeaderText');
      case media_router.MediaRouterView.ISSUE:
        return this.i18n('issueHeaderText');
      case media_router.MediaRouterView.ROUTE_DETAILS:
        return this.currentRoute_ && this.sinkMap_[this.currentRoute_.sinkId] ?
            this.sinkMap_[this.currentRoute_.sinkId].name :
            '';
      case media_router.MediaRouterView.SINK_LIST:
      case media_router.MediaRouterView.FILTER:
        return this.headerText;
      default:
        return '';
    }
  },

  /**
   * @param {?media_router.MediaRouterView} view The current view.
   * @param {string} headerTooltip The tooltip for the header for the sink
   *     list.
   * @return {string} The tooltip for the header.
   * @private
   */
  computeHeaderTooltip_: function(view, headerTooltip) {
    return view == media_router.MediaRouterView.SINK_LIST ? headerTooltip : '';
  },

  /**
   * @param {string} currentLaunchingSinkId ID of the sink that is currently
   *     launching, or empty string if none exists.
   * @private
   */
  computeIsLaunching_: function(currentLaunchingSinkId) {
    return currentLaunchingSinkId != '';
  },

  /**
   * @param {?media_router.Issue} issue The current issue.
   * @return {string} The class for the issue banner.
   * @private
   */
  computeIssueBannerClass_: function(issue) {
    return !!issue && !issue.isBlocking ? 'non-blocking' : '';
  },

  /**
   * @param {?media_router.MediaRouterView} view The current view.
   * @param {?media_router.Issue} issue The current issue.
   * @return {boolean} Whether or not to show the issue banner.
   * @private
   */
  computeIssueBannerShown_: function(view, issue) {
    return !!issue &&
        (view == media_router.MediaRouterView.CAST_MODE_LIST ||
         view == media_router.MediaRouterView.SINK_LIST ||
         view == media_router.MediaRouterView.FILTER ||
         view == media_router.MediaRouterView.ISSUE);
  },

  /**
   * @param {!Array<!{sinkItem: !media_router.Sink,
   *                  substrings: Array<!Array<number>>}>} searchResultsToShow
   *     The sinks currently matching the search text.
   * @param {boolean} isSearchListHidden Whether the search list is hidden.
   * @return {boolean} Whether or not the 'no matches' message is hidden.
   * @private
   */
  computeNoMatchesHidden_: function(searchResultsToShow, isSearchListHidden) {
    return isSearchListHidden || this.searchInputText_.length == 0 ||
        searchResultsToShow.length != 0;
  },

  /**
   * @param {!Array<!media_router.CastMode>} castModeList The current list of
   *     cast modes.
   * @return {!Array<!media_router.CastMode>} The list of non-PRESENTATION cast
   *     modes. Also excludes LOCAL_FILE.
   * @private
   */
  computeShareScreenCastModeList_: function(castModeList) {
    return castModeList.filter(function(mode) {
      return mode.type == media_router.CastModeType.DESKTOP_MIRROR ||
          mode.type == media_router.CastModeType.TAB_MIRROR;
    });
  },

  /**
   * @param {!Array<!media_router.CastMode>} castModeList The current list of
   *     cast modes.
   * @return {!Array<!media_router.CastMode>} The list of local media cast
   *     modes.
   * @private
   */
  computeLocalMediaCastModeList_: function(castModeList) {
    return castModeList.filter(function(mode) {
      return mode.type == media_router.CastModeType.LOCAL_FILE;
    });
  },

  /**
   * @param {?media_router.MediaRouterView} view The current view.
   * @param {?media_router.Issue} issue The current issue.
   * @return {boolean} Whether or not to hide the route details.
   * @private
   */
  computeRouteDetailsHidden_: function(view, issue) {
    return view != media_router.MediaRouterView.ROUTE_DETAILS ||
        (!!issue && issue.isBlocking);
  },

  /**
   * Computes an array of substring indices that mark where substrings of
   * |searchString| occur in |sinkName|.
   *
   * @param {string} searchString Search string entered by user.
   * @param {string} sinkName Sink name being filtered.
   * @return {Array<!Array<number>>} Array of substring start-end (inclusive)
   *     index pairs if every character in |searchString| was matched, in order,
   *     in |sinkName|. Otherwise it returns null.
   * @private
   */
  computeSearchMatches_: function(searchString, sinkName) {
    var i = 0;
    var matchStart = -1;
    var matchEnd = -1;
    var matchPairs = [];
    for (var j = 0; i < searchString.length && j < sinkName.length; ++j) {
      if (searchString[i].toLocaleLowerCase() ==
          sinkName[j].toLocaleLowerCase()) {
        if (matchStart == -1) {
          matchStart = j;
        }
        ++i;
      } else if (matchStart != -1) {
        matchEnd = j - 1;
        matchPairs.push([matchStart, matchEnd]);
        matchStart = -1;
      }
    }
    if (matchStart != -1) {
      matchEnd = j - 1;
      matchPairs.push([matchStart, matchEnd]);
    }
    return (i == searchString.length) ? matchPairs : null;
  },

  /**
   * Computes whether the search results list should be hidden.
   * @param {!Array<!{sinkItem: !media_router.Sink,
   *                  substrings: Array<!Array<number>>}>} searchResultsToShow
   *     The sinks currently matching the search text.
   * @param {boolean} isSearchListHidden Whether the search list is hidden.
   * @return {boolean} Whether the search results list should be hidden.
   * @private
   */
  computeSearchResultsHidden_: function(
      searchResultsToShow, isSearchListHidden) {
    return isSearchListHidden || searchResultsToShow.length == 0;
  },

  /**
   * @param {!Array<!media_router.CastMode>} castModeList The current list of
   *     cast modes.
   * @return {boolean} Whether or not to hide the share screen subheading text.
   * @private
   */
  computeShareScreenSubheadingHidden_: function(castModeList) {
    return this.computeShareScreenCastModeList_(castModeList).length == 0;
  },

  /**
   * @param {!Array<!media_router.CastMode>} castModeList The current list of
   *     cast modes.
   * @return {boolean} Whether or not to hide the local media subheading text.
   * @private
   */
  computeLocalMediaSubheadingHidden_: function(castModeList) {
    return this.computeLocalMediaCastModeList_(castModeList).length == 0;
  },

  /**
   * @param {boolean} showFirstRunFlow Whether or not to show the first run
   *     flow.
   * @param {?media_router.MediaRouterView} currentView The current view.
   * @private
   */
  computeShowFirstRunFlow_: function(showFirstRunFlow, currentView) {
    return showFirstRunFlow &&
        currentView == media_router.MediaRouterView.SINK_LIST;
  },

  /**
   * @param {!media_router.Sink} sink The sink to determine an icon for.
   * @return {string} The icon to use.
   * @private
   */
  computeSinkIcon_: function(sink) {
    switch (sink.iconType) {
      case media_router.SinkIconType.CAST:
        return 'media-router:chromecast';
      case media_router.SinkIconType.CAST_AUDIO_GROUP:
        return 'media-router:speaker-group';
      case media_router.SinkIconType.CAST_AUDIO:
        return 'media-router:speaker';
      case media_router.SinkIconType.MEETING:
        return 'media-router:meeting';
      case media_router.SinkIconType.HANGOUT:
        return 'media-router:hangout';
      case media_router.SinkIconType.EDUCATION:
        return 'media-router:education';
      case media_router.SinkIconType.WIRED_DISPLAY:
        return 'media-router:tv';
      case media_router.SinkIconType.GENERIC:
        return 'media-router:tv';
      default:
        return 'media-router:tv';
    }
  },

  /**
   * @param {!string} sinkId A sink ID.
   * @param {!Object<!string, ?media_router.Route>} sinkToRouteMap
   *     Maps media_router.Sink.id to corresponding media_router.Route.
   * @return {string} The class for the sink icon.
   * @private
   */
  computeSinkIconClass_: function(sinkId, sinkToRouteMap) {
    return sinkToRouteMap[sinkId] ? 'sink-icon active-sink' : 'sink-icon';
  },

  /**
   * @param {!string} currentLaunchingSinkId The ID of the sink that is
   *     currently launching.
   * @param {!string} sinkId A sink ID.
   * @return {boolean} |true| if given sink is currently launching.
   * @private
   */
  computeSinkIsLaunching_: function(currentLaunchingSinkId, sinkId) {
    return currentLaunchingSinkId == sinkId;
  },

  /**
   * @param {!Array<!media_router.Sink>} sinksToShow The list of sinks.
   * @return {boolean} Whether or not to hide the sink list.
   * @private
   */
  computeSinkListHidden_: function(sinksToShow) {
    return sinksToShow.length == 0;
  },

  /**
   * @param {?media_router.MediaRouterView} view The current view.
   * @param {?media_router.Issue} issue The current issue.
   * @return {boolean} Whether or not to hide entire the sink list view.
   * @private
   */
  computeSinkListViewHidden_: function(view, issue) {
    return (view != media_router.MediaRouterView.SINK_LIST &&
            view != media_router.MediaRouterView.FILTER) ||
        (!!issue && issue.isBlocking);
  },

  /**
   * Returns whether the sink domain for |sink| should be hidden.
   * @param {!media_router.Sink} sink
   * @return {boolean} |true| if the domain should be hidden.
   * @private
   */
  computeSinkDomainHidden_: function(sink) {
    return !this.showDomain || this.isEmptyOrWhitespace_(sink.domain);
  },

  /**
   * Computes which portions of a sink name, if any, should be highlighted when
   * displayed in the filter view. Any substrings matching the search text
   * should be highlighted.
   *
   * The order the strings are combined is plainText[0] highlightedText[0]
   * plainText[1] highlightedText[1] etc.
   *
   * @param {!{sinkItem: !media_router.Sink,
   *           substrings: !Array<!Array<number>>}} matchedItem
   * Parameters in matchedItem:
   *   sinkItem - Original !media_router.Sink from the sink list.
   *   substrings - List of index pairs denoting substrings of sinkItem.name
   *       that match |searchInputText_|.
   * @return {!{highlightedText: !Array<string>, plainText: !Array<string>}}
   *   highlightedText - Array of strings that should be displayed highlighted.
   *   plainText - Array of strings that should be displayed normally.
   * @private
   */
  computeSinkMatchingText_: function(matchedItem) {
    if (!matchedItem.substrings) {
      return {highlightedText: [null], plainText: [matchedItem.sinkItem.name]};
    }
    var lastMatchIndex = -1;
    var nameIndex = 0;
    var sinkName = matchedItem.sinkItem.name;
    var highlightedText = [];
    var plainText = [];
    for (var i = 0; i < matchedItem.substrings.length; ++i) {
      var [matchStart, matchEnd] = matchedItem.substrings[i];
      if (lastMatchIndex + 1 < matchStart) {
        plainText.push(sinkName.substring(lastMatchIndex + 1, matchStart));
      } else {
        plainText.push(null);
      }
      highlightedText.push(sinkName.substring(matchStart, matchEnd + 1));
      lastMatchIndex = matchEnd;
    }
    if (lastMatchIndex + 1 < sinkName.length) {
      highlightedText.push(null);
      plainText.push(sinkName.substring(lastMatchIndex + 1));
    }
    return {highlightedText: highlightedText, plainText: plainText};
  },

  /**
   * Returns the subtext to be shown for |sink|. Only called if
   * |computeSinkSubtextHidden_| returns false for the same |sink| and
   * |sinkToRouteMap|.
   * @param {!media_router.Sink} sink
   * @param {!Object<!string, ?media_router.Route>} sinkToRouteMap
   * @return {?string} The subtext to be shown.
   * @private
   */
  computeSinkSubtext_: function(sink, sinkToRouteMap) {
    var route = sinkToRouteMap[sink.id];
    if (route && !this.isEmptyOrWhitespace_(route.description))
      return route.description;

    return sink.description;
  },

  /**
   * Returns whether the sink subtext for |sink| should be hidden.
   * @param {!media_router.Sink} sink
   * @param {!Object<!string, ?media_router.Route>} sinkToRouteMap
   * @return {boolean} |true| if the subtext should be hidden.
   * @private
   */
  computeSinkSubtextHidden_: function(sink, sinkToRouteMap) {
    if (!this.isEmptyOrWhitespace_(sink.description))
      return false;

    var route = sinkToRouteMap[sink.id];
    return !route || this.isEmptyOrWhitespace_(route.description);
  },

  /**
   * @param {boolean} justOpened Whether the MR UI was just opened.
   * @return {boolean} Whether or not to hide the spinner.
   * @private
   */
  computeSpinnerHidden_: function(justOpened) {
    return !justOpened;
  },

  /**
   * Computes the height of the sink list view element when search results are
   * being shown.
   *
   * @param {?Element} deviceMissing No devices message element.
   * @param {?Element} noMatches No search matches element.
   * @param {?Element} results Search results list element.
   * @param {number} searchOffsetHeight Search input container element height.
   * @param {number} maxHeight Max height of the list elements.
   * @return {number} The height of the sink list view when search results are
   *     being shown.
   * @private
   */
  computeTotalSearchHeight_: function(
      deviceMissing, noMatches, results, searchOffsetHeight, maxHeight) {
    var contentHeight = deviceMissing.offsetHeight +
        ((noMatches.hasAttribute('hidden')) ? results.offsetHeight :
                                              noMatches.offsetHeight);
    return Math.min(contentHeight, maxHeight) + searchOffsetHeight;
  },

  /**
   * Updates element positioning when the view changes and possibly triggers
   * reporting of a user filter action. If there is no filter text, it defers
   * the reporting until some text is entered, but otherwise it reports the
   * filter action here.
   * @param {?media_router.MediaRouterView} currentView The current view of the
   *     dialog.
   * @param {?media_router.MediaRouterView} previousView The previous
   *     |currentView|.
   * @private
   */
  currentViewChanged_: function(currentView, previousView) {
    if (currentView == media_router.MediaRouterView.FILTER) {
      this.reportFilterOnInput_ = true;
      this.maybeReportFilter_();
    }
    this.updateElementPositioning_();

    if (previousView == media_router.MediaRouterView.ROUTE_DETAILS) {
      media_router.browserApi.onMediaControllerClosed();
      if (this.$$('route-details'))
        this.$$('route-details').onClosed();
    }
  },

  /**
   * Filters all sinks based on fuzzy matching to the currently entered search
   * text.
   * @param {string} searchInputText The currently entered search text.
   * @private
   */
  filterSinks_: function(searchInputText) {
    if (searchInputText.length == 0) {
      this.searchResultsToShow_ = this.sinksToShow_.map(function(item) {
        return {sinkItem: item, substrings: null};
      });
      return;
    }

    var searchResultsToShow = [];
    for (var i = 0; i < this.sinksToShow_.length; ++i) {
      var matchSubstrings = this.computeSearchMatches_(
          searchInputText, this.sinksToShow_[i].name);
      if (!matchSubstrings) {
        continue;
      }
      searchResultsToShow.push(
          {sinkItem: this.sinksToShow_[i], substrings: matchSubstrings});
    }
    searchResultsToShow.sort(this.compareSearchMatches_);

    var pendingPseudoSink = (this.pseudoSinkSearchState_) ?
        this.pseudoSinkSearchState_.getPseudoSink() :
        null;
    // We may need to add pseudo sinks to the filter results. A pseudo sink will
    // be shown if there is no real sink with the same icon and name exactly
    // matching the filter text. The map() call transforms any pseudo sink
    // objects that will be shown to the search result format, where we know
    // that the entire sink name will be a match.
    //
    // The exception to this is when there is a pending pseudo sink search. Then
    // the pseudo sink for the search will be treated like a real sink because
    // it will actually be in |sinksToShow_| until a real sink is returned by
    // search. So the filter here shouldn't treat it like a pseudo sink.
    searchResultsToShow =
        this.pseudoSinks_
            .filter(function(pseudoSink) {
              return (!pendingPseudoSink ||
                      pseudoSink.id != pendingPseudoSink.id) &&
                  !searchResultsToShow.find(function(searchResult) {
                    return searchResult.sinkItem.name == searchInputText &&
                        searchResult.sinkItem.iconType == pseudoSink.iconType;
                  });
            })
            .map(function(pseudoSink) {
              pseudoSink.name = searchInputText;
              return {
                sinkItem: pseudoSink,
                substrings: [[0, searchInputText.length - 1]]
              };
            })
            .concat(searchResultsToShow);
    this.searchResultsToShow_ = searchResultsToShow;
  },

  /**
   * Helper function to locate the CastMode object with the given type in
   * castModeList.
   *
   * @param {number} castModeType Type of cast mode to look for.
   * @return {media_router.CastMode|undefined} CastMode object with the given
   *     type in castModeList, or undefined if not found.
   * @private
   */
  findCastModeByType_: function(castModeType) {
    return this.castModeList.find(function(element, index, array) {
      return element.type == castModeType;
    });
  },

  /**
   * Helper function to locate the position in the |castModeList| of the
   * CastMode object with the given type.
   *
   * @param {number} castModeType Type of cast mode to look for.
   * @return {number} index of the given type, or -1 if not found.
   * @private
   */
  findCastModeIndexByType_: function(castModeType) {
    return this.castModeList
        .map(function(element) {
          return element.type;
        })
        .indexOf(castModeType);
  },


  /**
   * Helper function to return a forced CastMode, if any.
   *
   * @return {media_router.CastMode|undefined} CastMode object with
   *     isForced = true, or undefined if not found.
   * @private
   */
  findForcedCastMode_: function() {
    return this.castModeList &&
        this.castModeList.find(element => element.isForced);
  },

  /**
   * @param {?Element} element Element to compute padding for.
   * @return {!Array<number>} Array containing the element's bottom padding
   *     value and the element's top padding value, in that order.
   * @private
   */
  getElementVerticalPadding_: function(element) {
    var style = window.getComputedStyle(element);
    return [
      parseInt(style.getPropertyValue('padding-bottom'), 10) || 0,
      parseInt(style.getPropertyValue('padding-top'), 10) || 0
    ];
  },

  /**
   * Retrieves the first run flow cloud preferences text, if it exists. On
   * non-officially branded builds, the string is not defined.
   *
   * @return {string} Cloud preferences text.
   */
  getFirstRunFlowCloudPrefText_: function() {
    return loadTimeData.valueExists('firstRunFlowCloudPrefText') ?
        this.i18n('firstRunFlowCloudPrefText') :
        '';
  },

  /**
   * @param {?media_router.Route} route Route to get the sink for.
   * @return {?media_router.Sink} Sink associated with |route| or
   *     undefined if we don't have data for the sink.
   */
  getSinkForRoute_: function(route) {
    return route ? this.sinkMap_[route.sinkId] : null;
  },

  /**
   * @param {?Element} element Conditionally-templated element to check.
   * @return {boolean} Whether |element| is considered present in the document
   *     as a conditionally-templated element. This does not check the |hidden|
   *     attribute.
   */
  hasConditionalElement_: function(element) {
    return !!element &&
        (!element.style.display || element.style.display != 'none');
  },

  /**
   * Returns whether given string is undefined, null, empty, or whitespace only.
   * @param {?string} str String to be tested.
   * @return {boolean} |true| if the string is undefined, null, empty, or
   *     whitespace.
   * @private
   */
  isEmptyOrWhitespace_: function(str) {
    return str === undefined || str === null || (/^\s*$/).test(str);
  },

  /**
   * Reports a user filter action if |searchInputText_| is not empty and the
   * filter action hasn't been reported since the view changed to the filter
   * view.
   * @private
   */
  maybeReportFilter_: function() {
    if (this.reportFilterOnInput_ && this.searchInputText_.length != 0) {
      this.reportFilterOnInput_ = false;
      this.fire('report-filter');
    }
  },

  /**
   * Updates |currentView_| if the dialog had just opened and there's
   * only one local route.
   */
  maybeShowRouteDetailsOnOpen: function() {
    var localRoute = null;
    for (var i = 0; i < this.routeList.length; i++) {
      var route = this.routeList[i];
      if (!route.isLocal)
        continue;
      if (!localRoute) {
        localRoute = route;
      } else {
        // Don't show route details if there are more than one local route.
        localRoute = null;
        break;
      }
    }

    if (localRoute)
      this.showRouteDetails_(localRoute);
    this.fire('show-initial-state', {currentView: this.currentView_});
  },

  /**
   * Updates |currentView_| if there is a new blocking issue or a blocking
   * issue is resolved. Clears any pending route creation properties if the
   * issue corresponds with |pendingCreatedRouteId_|.
   *
   * @param {?media_router.Issue} issue The new issue, or null if the
   *                              blocking issue was resolved.
   * @private
   */
  maybeShowIssueView_: function(issue) {
    if (!!issue) {
      if (issue.isBlocking) {
        this.currentView_ = media_router.MediaRouterView.ISSUE;
      } else if (this.currentView_ == media_router.MediaRouterView.SINK_LIST) {
        // Make space for the non-blocking issue in the sink list.
        this.updateElementPositioning_();
      }
    } else if (this.currentView_ == media_router.MediaRouterView.ISSUE) {
      // Switch back to the sink list if the issue was cleared and it was
      // showing an issue. It is expected that the only way to clear an issue is
      // by user action; the IssueManager (C++ side) does not clear issues in
      // the UI.
      this.showSinkList_();
    }

    if (!!this.pendingCreatedRouteId_ && !!issue &&
        issue.routeId == this.pendingCreatedRouteId_) {
      this.resetRouteCreationProperties_(false);
    }
  },

  /**
   * If an element in the search results list has keyboard focus when we are
   * transitioning from the filter view to the sink list view, give focus to the
   * same sink in the sink list. Otherwise we leave the keyboard focus where it
   * is.
   * @private
   */
  maybeUpdateFocusOnFilterViewExit_: function() {
    var searchSinks =
        this.$$('#search-results').querySelectorAll('.selectable-item');
    var focusedElem = Array.prototype.find.call(searchSinks, function(sink) {
      return sink.matches(':focus');
    });
    if (!focusedElem) {
      return;
    }
    var focusedSink =
        this.$$('#searchResults').itemForElement(focusedElem).sinkItem;
    setTimeout(function() {
      var sinkListPaperMenu = this.$$('#sink-list-paper-menu');
      var sinks = sinkListPaperMenu.children;
      var sinkList = this.$$('#sinkList');
      for (var i = 0; i < sinks.length; i++) {
        if (sinkList.itemForElement(sinks[i]).id == focusedSink.id) {
          sinkListPaperMenu.selectIndex(i);
          break;
        }
      }
    }.bind(this));
  },

  /**
   * May update |populatedSinkListSeenTimeMs_| depending on |currentView| and
   * |sinksToShow|.
   * Called when |currentView_| or |sinksToShow_| is updated.
   *
   * @param {?media_router.MediaRouterView} currentView The current view of the
   *                                        dialog.
   * @param {!Array<!media_router.Sink>} sinksToShow The sinks to display.
   * @private
   */
  maybeUpdateStartSinkDisplayStartTime_: function(currentView, sinksToShow) {
    if (currentView == media_router.MediaRouterView.SINK_LIST &&
        sinksToShow.length != 0) {
      // Only set |populatedSinkListSeenTimeMs_| if it has not already been set.
      if (this.populatedSinkListSeenTimeMs_ == -1)
        this.populatedSinkListSeenTimeMs_ = window.performance.now();
    } else {
      // Reset |populatedSinkListLastSeen_| if the sink list isn't being shown
      // or if there aren't any sinks available for display.
      this.populatedSinkListSeenTimeMs_ = -1;
    }
  },

  /**
   * Animates the transition from the filter view, where the search field is at
   * the top of the list, to the sink list view, where the search field is at
   * the bottom of the list.
   *
   * If this is called while another animation is in progress, it queues itself
   * to be run at the end of the current animation.
   *
   * @param {!function()} resolve Resolves the animation promise that is waiting
   *     on this animation.
   * @private
   */
  moveSearchToBottom_: function(resolve) {
    var deviceMissing = this.$['device-missing'];
    var list = this.$$('#sink-list');
    var resultsContainer = this.$$('#search-results-container');
    var search = this.$$('#sink-search');
    var view = this.$['sink-list-view'];

    var hasList = this.hasConditionalElement_(list);
    var initialHeight = view.offsetHeight;
    // Force the view height to be max dialog height.
    view.style['overflow'] = 'hidden';

    var searchInitialOffsetHeight = search.offsetHeight;
    var searchInitialPaddingBottom, searchInitialPaddingTop;
    [searchInitialPaddingBottom, searchInitialPaddingTop] =
        this.getElementVerticalPadding_(search);
    var searchPadding = searchInitialPaddingBottom + searchInitialPaddingTop;
    var searchHeight = search.offsetHeight - searchPadding;
    var searchFinalPaddingBottom, searchFinalPaddingTop;
    [searchFinalPaddingBottom, searchFinalPaddingTop] =
        this.getElementVerticalPadding_(search);
    var searchFinalOffsetHeight =
        searchHeight + searchFinalPaddingBottom + searchFinalPaddingTop;

    var resultsInitialTop = 0;
    var finalHeight = 0;
    // Get final view height ahead of animation.
    if (hasList) {
      list.style['position'] = 'absolute';
      list.style['opacity'] = '0';
      this.hideSinkListForAnimation_ = false;
      finalHeight += list.offsetHeight;
      list.style['position'] = 'relative';
    } else {
      resultsInitialTop +=
          deviceMissing.offsetHeight + searchInitialOffsetHeight;
      finalHeight += deviceMissing.offsetHeight;
    }

    var searchInitialTop = hasList ? 0 : deviceMissing.offsetHeight;
    var searchFinalTop = hasList ? list.offsetHeight - search.offsetHeight :
                                   deviceMissing.offsetHeight;
    resultsContainer.style['position'] = 'absolute';

    var duration =
        this.computeAnimationDuration_(searchFinalTop - searchInitialTop);
    var timing = {duration: duration, easing: 'ease-in-out', fill: 'forwards'};

    // This GroupEffect does the reverse of |moveSearchToTop_|. It fades the
    // sink list in while sliding the search input and search results list down.
    // The dialog height is also adjusted smoothly to the sink list height.
    var deviceMissingEffect = new KeyframeEffect(
        deviceMissing,
        [
          {'marginBottom': searchInitialOffsetHeight},
          {'marginBottom': searchFinalOffsetHeight}
        ],
        timing);
    var listEffect =
        new KeyframeEffect(list, [{'opacity': '0'}, {'opacity': '1'}], timing);
    var resultsEffect = new KeyframeEffect(
        resultsContainer,
        [
          {
            'top': resultsInitialTop + 'px',
            'paddingTop': resultsContainer.style['padding-top']
          },
          {'top': '100%', 'paddingTop': '0px'}
        ],
        timing);
    var searchEffect = new KeyframeEffect(
        search,
        [
          {
            'top': searchInitialTop + 'px',
            'marginTop': '0px',
            'paddingBottom': searchInitialPaddingBottom + 'px',
            'paddingTop': searchInitialPaddingTop + 'px'
          },
          {
            'top': '100%',
            'marginTop': '-' + searchFinalOffsetHeight + 'px',
            'paddingBottom': searchFinalPaddingBottom + 'px',
            'paddingTop': searchFinalPaddingTop + 'px'
          }
        ],
        timing);
    var viewEffect = new KeyframeEffect(
        view,
        [
          {'height': initialHeight + 'px', 'paddingBottom': '0px'}, {
            'height': finalHeight + 'px',
            'paddingBottom': searchFinalOffsetHeight + 'px'
          }
        ],
        timing);
    var player = document.timeline.play(new GroupEffect(
        hasList ?
            [listEffect, resultsEffect, searchEffect, viewEffect] :
            [deviceMissingEffect, resultsEffect, searchEffect, viewEffect]));

    var that = this;
    var finalizeAnimation = function() {
      view.style['overflow'] = '';
      that.putSearchAtBottom_();
      that.filterTransitionPlayer_.cancel();
      that.filterTransitionPlayer_ = null;
      that.isSearchListHidden_ = true;
      resolve();
    };

    player.finished.then(finalizeAnimation);
    this.filterTransitionPlayer_ = player;
  },

  /**
   * Animates the transition from the sink list view, where the search field is
   * at the bottom of the list, to the filter view, where the search field is at
   * the top of the list.
   *
   * If this is called while another animation is in progress, it queues itself
   * to be run at the end of the current animation.
   *
   * @param {!function()} resolve Resolves the animation promise that is waiting
   *     on this animation.
   * @private
   */
  moveSearchToTop_: function(resolve) {
    var deviceMissing = this.$['device-missing'];
    var list = this.$$('#sink-list');
    var noMatches = this.$$('#no-search-matches');
    var results = this.$$('#search-results');
    var resultsContainer = this.$$('#search-results-container');
    var search = this.$$('#sink-search');
    var view = this.$['sink-list-view'];

    // Set the max height for the results list before it's shown.
    results.style.maxHeight = this.sinkListMaxHeight_ + 'px';

    // Saves current search container |offsetHeight| which includes bottom
    // padding.
    var searchInitialOffsetHeight = search.offsetHeight;
    var hasList = this.hasConditionalElement_(list);
    var searchInitialTop = hasList ?
        list.offsetHeight - searchInitialOffsetHeight :
        deviceMissing.offsetHeight;
    var searchFinalTop = hasList ? 0 : deviceMissing.offsetHeight;
    var searchInitialPaddingBottom, searchInitialPaddingTop;
    [searchInitialPaddingBottom, searchInitialPaddingTop] =
        this.getElementVerticalPadding_(search);
    var searchPadding = searchInitialPaddingBottom + searchInitialPaddingTop;
    var searchHeight = search.offsetHeight - searchPadding;
    var searchFinalPaddingBottom, searchFinalPaddingTop;
    [searchFinalPaddingBottom, searchFinalPaddingTop] =
        this.getElementVerticalPadding_(search);
    var searchFinalOffsetHeight =
        searchHeight + searchFinalPaddingBottom + searchFinalPaddingTop;

    // Omitting |search.offsetHeight| because it is handled by view animation
    // separately.
    var initialHeight =
        hasList ? list.offsetHeight : deviceMissing.offsetHeight;
    view.style['overflow'] = 'hidden';

    var resultsPadding = this.computeElementVerticalPadding_(results);
    var finalHeight = this.computeTotalSearchHeight_(
        deviceMissing, noMatches, results, searchFinalOffsetHeight,
        this.sinkListMaxHeight_ + resultsPadding);

    var duration =
        this.computeAnimationDuration_(searchFinalTop - searchInitialTop);
    var timing = {duration: duration, easing: 'ease-in-out', fill: 'forwards'};

    // This GroupEffect will cause the sink list to fade out while the search
    // input and search results list slide up. The dialog will also resize
    // smoothly to the new search result list height.
    var deviceMissingEffect = new KeyframeEffect(
        deviceMissing,
        [
          {'marginBottom': searchInitialOffsetHeight},
          {'marginBottom': searchFinalOffsetHeight}
        ],
        timing);
    var listEffect =
        new KeyframeEffect(list, [{'opacity': '1'}, {'opacity': '0'}], timing);
    var resultsEffect = new KeyframeEffect(
        resultsContainer,
        [
          {'top': '100%', 'paddingTop': '0px'}, {
            'top': searchFinalTop + 'px',
            'paddingTop': searchFinalOffsetHeight + 'px'
          }
        ],
        timing);
    var searchEffect = new KeyframeEffect(
        search,
        [
          {
            'top': '100%',
            'marginTop': '-' + searchInitialOffsetHeight + 'px',
            'paddingBottom': searchInitialPaddingBottom + 'px',
            'paddingTop': searchInitialPaddingTop + 'px'
          },
          {
            'top': searchFinalTop + 'px',
            'marginTop': '0px',
            'paddingBottom': searchFinalPaddingBottom + 'px',
            'paddingTop': searchFinalPaddingTop + 'px'
          }
        ],
        timing);
    var viewEffect = new KeyframeEffect(
        view,
        [
          {
            'height': initialHeight + 'px',
            'paddingBottom': searchInitialOffsetHeight + 'px'
          },
          {'height': finalHeight + 'px', 'paddingBottom': '0px'}
        ],
        timing);
    var player = document.timeline.play(new GroupEffect(
        hasList ?
            [listEffect, resultsEffect, searchEffect, viewEffect] :
            [deviceMissingEffect, resultsEffect, searchEffect, viewEffect]));

    var that = this;
    var finalizeAnimation = function() {
      // When we are moving the search results up into view, the user may type
      // more text or delete text which may change the height of the search
      // results list. In this case, the dialog height that the animation ends
      // on will now be wrong. In order to correct this smoothly,
      // |putSearchAtTop_| will queue another animation just to adjust the
      // dialog height.
      //
      // The |filterTransitionPlayer_| will hold all of the animated elements in
      // their final keyframe state until it is canceled or another player
      // overrides it because we used |fill: 'forwards'| in all of the effects.
      // So unlike |moveSearchToBottom_|, we don't know for sure whether we want
      // to cancel |filterTransitionPlayer_| after |putSearchAtTop_| because
      // another animation may have been run to correct the dialog height.
      //
      // If |putSearchAtTop_| has to adjust the dialog height, it also queues
      // itself to run again when that animation is finished. When the height is
      // finally correct at the end of an animation, it will cancel
      // |filterTransitionPlayer_| itself.
      that.putSearchAtTop_(resolve);
    };

    player.finished.then(finalizeAnimation);
    this.filterTransitionPlayer_ = player;
  },

  /**
   * Handles a cast mode selection. Updates |headerText|, |headerTextTooltip|,
   * and |shownCastModeValue_|.
   *
   * @param {!Event} event The event object.
   * @private
   */
  onCastModeClick_: function(event) {
    // The clicked cast mode can come from one of three lists,
    // presentationCastModeList, shareScreenCastModeList, and
    // localMediaCastModeList.
    var clickedMode =
        this.$$('#presentationCastModeList').itemForElement(event.target) ||
        this.$$('#shareScreenCastModeList').itemForElement(event.target) ||
        this.$$('#localMediaCastModeList').itemForElement(event.target);

    if (!clickedMode)
      return;

    // If the user selects LOCAL_FILE, some additional steps are required
    // (selecting the file), before the cast mode has been officially
    // selected.
    if (clickedMode.type == media_router.CastModeType.LOCAL_FILE) {
      this.selectLocalMediaFile_();
    } else {
      this.castModeSelected_(clickedMode);
    }
  },

  /**
   * Handles a change-route-source-click event. Sets the currently launching
   * sink to be the current route's sink and shows the sink list.
   *
   * @param {!Event} event The event object.
   * Parameters in |event|.detail:
   *   route - route to modify.
   *   selectedCastMode - cast mode to use for the new source.
   * @private
   */
  onChangeRouteSourceClick_: function(event) {
    /** @type {{route: !media_router.Route, selectedCastMode: number}} */
    var detail = event.detail;
    this.currentLaunchingSinkId_ = detail.route.sinkId;
    var sink = this.sinkMap_[detail.route.sinkId];
    this.showSinkList_();
    this.maybeReportUserFirstAction(
        media_router.MediaRouterUserAction.REPLACE_LOCAL_ROUTE);
  },

  /**
   * Handles a close-route event. Shows the sink list and starts a timer to
   * close the dialog if there is no click within three seconds.
   *
   * @param {!Event} event The event object.
   * Parameters in |event|.detail:
   *   route - route to close.
   * @private
   */
  onCloseRoute_: function(event) {
    /** @type {{route: media_router.Route}} */
    var detail = event.detail;
    this.showSinkList_();
    this.startTapTimer_();

    if (detail.route.isLocal) {
      this.maybeReportUserFirstAction(
          media_router.MediaRouterUserAction.STOP_LOCAL);
    }
  },

  /**
   * Handles response of previous create route attempt.
   *
   * @param {string} sinkId The ID of the sink to which the Media Route was
   *     creating a route.
   * @param {?media_router.Route} route The newly created route that
   *     corresponds to the sink if route creation succeeded; null otherwise.
   * @param {boolean} isForDisplay Whether or not |route| is for display.
   */
  onCreateRouteResponseReceived: function(sinkId, route, isForDisplay) {
    // The provider will handle sending an issue for a failed route request.
    if (!route) {
      this.resetRouteCreationProperties_(false);
      this.fire('report-resolved-route', {
        outcome: media_router.MediaRouterRouteCreationOutcome.FAILURE_NO_ROUTE
      });
      return;
    }

    // Check that |sinkId| exists and corresponds to |currentLaunchingSinkId_|.
    if (!this.sinkMap_[sinkId] || this.currentLaunchingSinkId_ != sinkId) {
      this.fire('report-resolved-route', {
        outcome:
            media_router.MediaRouterRouteCreationOutcome.FAILURE_INVALID_SINK
      });
      return;
    }

    // Regardless of whether the route is for display, it was resolved
    // successfully.
    this.fire(
        'report-resolved-route',
        {outcome: media_router.MediaRouterRouteCreationOutcome.SUCCESS});

    if (isForDisplay) {
      this.showRouteDetails_(route);
      this.startTapTimer_();
      this.resetRouteCreationProperties_(true);
    } else {
      this.pendingCreatedRouteId_ = route.id;
    }
  },

  /**
   * Sets up the LOCAL_FILE cast mode for display after a specific file has been
   * selected.
   *
   * @param {string} fileName The name of the file that has been selected.
   */
  onFileDialogSuccess(fileName) {
    /** @const */ var mode =
        this.findCastModeByType_(media_router.CastModeType.LOCAL_FILE);

    if (!mode)
      return;

    this.castModeSelected_(mode);
    this.headerText =
        loadTimeData.getStringF('castLocalMediaSelectedFileTitle', fileName);

    this.updateSelectedCastModeMenuItem_();
  },

  /**
   * Called when a focus event is triggered.
   *
   * @param {!Event} event The event object.
   * @private
   */
  onFocus_: function(event) {
    // If the focus event was automatically fired by Polymer, remove focus from
    // the element. This prevents unexpected focusing when the dialog is
    // initially loaded. This only happens on mac.
    if (cr.isMac && !event.sourceCapabilities) {
      // Adding a focus placeholder element is part of the workaround for
      // handling unexpected focusing, which only happens once on dialog open.
      // Since the placeholder is focus-enabled as denoted by its tabindex
      // value, the focus will not appear in other elements.
      var placeholder = this.$$('#focus-placeholder');
      // Check that the placeholder is the currently focused element. In some
      // tests, other elements are non-user-triggered focused.
      if (placeholder && this.shadowRoot.activeElement == placeholder) {
        event.path[0].blur();
        // Remove the placeholder since we have no more use for it.
        placeholder.remove();
      }
    }
  },

  /**
   * Called when a keydown event is fired.
   * @param {!Event} e Keydown event object for the event.
   */
  onKeydown_: function(e) {
    // The ESC key may be pressed with a combination of other keys. It is
    // handled on the C++ side instead of the JS side on non-mac platforms,
    // which uses toolkit-views. Handle the expected behavior on all platforms
    // here.
    if (e.key == media_router.KEY_ESC && !e.shiftKey && !e.ctrlKey &&
        !e.altKey && !e.metaKey) {
      // When searching, allow ESC as a mechanism to leave the filter view.
      if (this.currentView_ == media_router.MediaRouterView.FILTER) {
        // If the user tabbed to an item in the search results, or otherwise has
        // an item in the list focused, focus will seem to vanish when we
        // transition back to the sink list. Instead we should move focus to the
        // appropriate item in the sink list.
        this.maybeUpdateFocusOnFilterViewExit_();
        this.showSinkList_();
        e.preventDefault();
      } else {
        this.fire('close-dialog', {
          pressEscToClose: true,
        });
      }
    }
  },

  /**
   * Called when a mouseleave event is triggered.
   *
   * @private
   */
  onMouseLeave_: function() {
    this.mouseIsPositionedOverDialog_ = false;
  },

  /**
   * Called when a mouseenter event is triggered.
   *
   * @private
   */
  onMouseEnter_: function() {
    this.mouseIsPositionedOverDialog_ = true;
  },

  /**
   * Called when a search has completed up to route creation. |sinkId|
   * identifies the sink that should be in |allSinks|, if a sink was found.
   *
   * @param {string} sinkId The ID of the sink that is the result of the
   *     currently pending search.
   */
  onReceiveSearchResult: function(sinkId) {
    this.pseudoSinkSearchState_.receiveSinkResponse(sinkId);
    this.currentLaunchingSinkId_ =
        this.pseudoSinkSearchState_.checkForRealSink(this.allSinks);
    this.rebuildSinksToShow_();
    // If we're in filter view, make sure the |sinksToShow_| change is picked
    // up.
    if (this.currentView_ == media_router.MediaRouterView.FILTER) {
      this.filterSinks_(this.searchInputText_);
    }
  },

  /**
   * Called when the connection to the route controller is invalidated. Switches
   * from route details view to the sink list view.
   */
  onRouteControllerInvalidated: function() {
    if (this.currentView_ == media_router.MediaRouterView.ROUTE_DETAILS) {
      this.currentRoute_ = null;
      this.showSinkList_();
    }
  },

  /**
   * Called when a sink is clicked.
   *
   * @param {!Event} event The event object.
   * @private
   */
  onSinkClick_: function(event) {
    var clickedSink =
        (this.currentView_ == media_router.MediaRouterView.FILTER) ?
        this.$$('#searchResults').itemForElement(event.target).sinkItem :
        this.$$('#sinkList').itemForElement(event.target);
    this.showOrCreateRoute_(clickedSink);
    this.fire('sink-click', {index: event['model'].index});
  },

  /**
   * Sets the positioning of the sink list, search input, and search results so
   * that everything is in the correct state for the sink list view.
   *
   * @private
   */
  putSearchAtBottom_: function() {
    var search = this.$$('#sink-search');
    if (!this.hasConditionalElement_(search)) {
      return;
    }
    var deviceMissing = this.$['device-missing'];
    var list = this.$$('#sink-list');
    var resultsContainer = this.$$('#search-results-container');
    var view = this.$['sink-list-view'];
    search.style['top'] = '';
    if (resultsContainer) {
      resultsContainer.style['position'] = '';
      resultsContainer.style['padding-top'] = '';
      resultsContainer.style['top'] = '';
    }
    this.hideSinkListForAnimation_ = false;
    var hasList = this.hasConditionalElement_(list);
    if (hasList) {
      search.style['margin-top'] = '-' + search.offsetHeight + 'px';
      view.style['padding-bottom'] = search.offsetHeight + 'px';
      list.style['opacity'] = '';
    } else {
      var bottomMargin = 12;
      deviceMissing.style['margin-bottom'] =
          (search.offsetHeight + bottomMargin) + 'px';
      search.style['margin-top'] = '';
      view.style['padding-bottom'] = '';
    }
  },

  /**
   * Sets the positioning of the sink list, search input, and search results so
   * that everything is in the correct state for the filter view.
   *
   * If the user was searching while the |moveSearchToTop_| animation was
   * happening then the dialog height that animation ends at could be different
   * than the current height of the search results. If this is the case, this
   * function first spawns a new animation that smoothly corrects the height
   * problem. This is iterative, but once we enter a call where the heights
   * match up, the elements will become static again.
   *
   * @param {!function()} resolve Resolves the animation promise that is waiting
   *     on this animation.
   * @private
   */
  putSearchAtTop_: function(resolve) {
    var deviceMissing = this.$['device-missing'];
    var list = this.$$('#sink-list');
    var noMatches = this.$$('#no-search-matches');
    var results = this.$$('#search-results');
    var resultsContainer = this.$$('#search-results-container');
    var search = this.$$('#sink-search');
    var view = this.$['sink-list-view'];

    // Set the max height for the results list before it's shown.
    results.style.maxHeight = this.sinkListMaxHeight_ + 'px';

    // If there is a height mismatch between where the animation calculated the
    // height should be and where it is now because the search results changed
    // during the animation, correct it with... another animation.
    var resultsPadding = this.computeElementVerticalPadding_(results);
    var finalHeight = this.computeTotalSearchHeight_(
        deviceMissing, noMatches, results, search.offsetHeight,
        this.sinkListMaxHeight_ + resultsPadding);
    if (finalHeight != view.offsetHeight) {
      var viewEffect = new KeyframeEffect(
          view,
          [
            {'height': view.offsetHeight + 'px'},
            {'height': finalHeight + 'px'}
          ],
          {
            duration:
                this.computeAnimationDuration_(finalHeight - view.offsetHeight),
            easing: 'ease-in-out',
            fill: 'forwards'
          });
      var player = document.timeline.play(viewEffect);
      if (this.heightAdjustmentPlayer_) {
        this.heightAdjustmentPlayer_.cancel();
      }
      this.heightAdjustmentPlayer_ = player;
      player.finished.then(this.putSearchAtTop_.bind(this, resolve));
      return;
    }

    var hasList = this.hasConditionalElement_(list);
    search.style['margin-top'] = '';
    deviceMissing.style['margin-bottom'] = search.offsetHeight + 'px';
    var searchFinalTop = hasList ? 0 : deviceMissing.offsetHeight;
    var resultsPaddingTop = hasList ? search.offsetHeight + 'px' : '0px';
    search.style['top'] = searchFinalTop + 'px';
    this.hideSinkListForAnimation_ = true;
    resultsContainer.style['position'] = 'relative';
    resultsContainer.style['padding-top'] = resultsPaddingTop;
    resultsContainer.style['top'] = '';

    view.style['overflow'] = '';
    view.style['padding-bottom'] = '';
    if (this.filterTransitionPlayer_) {
      this.filterTransitionPlayer_.cancel();
      this.filterTransitionPlayer_ = null;
    }

    if (this.heightAdjustmentPlayer_) {
      this.heightAdjustmentPlayer_.cancel();
      this.heightAdjustmentPlayer_ = null;
    }

    resolve();
  },

  /**
   * Queues a call to |moveSearchToBottom_| by adding it as a continuation to
   * |animationPromise_| and updating |animationPromise_|.
   */
  queueMoveSearchToBottom_: function() {
    var oldPromise = this.animationPromise_;
    var that = this;
    this.animationPromise_ = new Promise(function(resolve) {
      oldPromise.then(that.moveSearchToBottom_.bind(that, resolve));
    });
  },

  /**
   * Queues a call to |moveSearchToTop_| by adding it as a continuation to
   * |animationPromise_| and updating |animationPromise_|. The new promise will
   * not resolve until |putSearchAtTop_| is finished, including any potential
   * dialog height adjustment animations.
   */
  queueMoveSearchToTop_: function() {
    var oldPromise = this.animationPromise_;
    var that = this;
    this.animationPromise_ = new Promise(function(resolve) {
      oldPromise.then(function() {
        that.isSearchListHidden_ = false;
        setTimeout(that.moveSearchToTop_.bind(that, resolve));
      });
    });
  },

  /**
   * Queues a call to |putSearchAtTop_| by adding it as a continuation to
   * |animationPromise_| and updating |animationPromise_|.
   */
  queuePutSearchAtTop_: function() {
    var that = this;
    var oldPromise = this.animationPromise_;
    this.animationPromise_ = new Promise(function(resolve) {
      oldPromise.then(that.putSearchAtTop_.bind(that, resolve));
    });
  },

  /**
   * Called when |routeList| is updated. Rebuilds |routeMap_| and
   * |sinkToRouteMap_|.
   *
   * @private
   */
  rebuildRouteMaps_: function() {
    this.routeMap_ = {};

    // Rebuild |sinkToRouteMap_| with a temporary map to avoid firing the
    // computed functions prematurely.
    var tempSinkToRouteMap = {};

    // We expect that each route in |routeList| maps to a unique sink.
    this.routeList.forEach(function(route) {
      this.routeMap_[route.id] = route;
      tempSinkToRouteMap[route.sinkId] = route;
    }, this);

    // If there is route creation in progress, check if any of the route ids
    // correspond to |pendingCreatedRouteId_|. If so, the newly created route
    // is ready to be displayed; switch to route details view.
    if (this.currentLaunchingSinkId_ != '' &&
        this.pendingCreatedRouteId_ != '') {
      var route = tempSinkToRouteMap[this.currentLaunchingSinkId_];
      if (route && this.pendingCreatedRouteId_ == route.id) {
        this.showRouteDetails_(route);
        this.startTapTimer_();
        this.resetRouteCreationProperties_(true);
      }
    } else {
      // If |currentRoute_| is no longer active, clear |currentRoute_|. Also
      // switch back to the SINK_PICKER view if the user is currently in the
      // ROUTE_DETAILS view.
      if (this.currentRoute_) {
        this.currentRoute_ = this.routeMap_[this.currentRoute_.id] || null;
      }
      if (!this.currentRoute_ &&
          this.currentView_ == media_router.MediaRouterView.ROUTE_DETAILS) {
        this.showSinkList_();
      }
    }

    this.sinkToRouteMap_ = tempSinkToRouteMap;
    this.rebuildSinksToShow_();
  },

  /**
   * Rebuilds the list of sinks to be shown for the current cast mode.
   * A sink should be shown if it is compatible with the current cast mode, or
   * if the sink is associated with a route.  The resulting list is sorted by
   * name.
   */
  rebuildSinksToShow_: function() {
    var updatedSinkList = this.allSinks.filter(function(sink) {
      return !sink.isPseudoSink;
    }, this);

    if (this.pseudoSinkSearchState_) {
      var pendingPseudoSink = this.pseudoSinkSearchState_.getPseudoSink();
      // Here we will treat the pseudo sink that launched the search as a real
      // sink until one is returned by search. This way it isn't possible to
      // ever reach a UI state where there is no spinner being shown in the sink
      // list but |currentLaunchingSinkId_| is non-empty (thereby preventing any
      // other sink from launching).
      if (pendingPseudoSink.id == this.currentLaunchingSinkId_) {
        updatedSinkList.unshift(pendingPseudoSink);
      }
    }
    // If user did not select a cast mode, then:
    // - If there is a forced cast mode, it is shown.
    // - If all sinks support only a single cast mode, then the cast mode is
    //   switched to that mode.
    // - Otherwise, the cast mode becomes AUTO mode.
    if (!this.userHasSelectedCastMode_)
      this.setShownCastMode_(this.computeCastMode_());

    // Non-AUTO modes may show a subset of sinks based on compatibility with the
    // shown value.
    if (this.shownCastModeValue_ != media_router.CastModeType.AUTO) {
      updatedSinkList = updatedSinkList.filter(function(element) {
        return (element.castModes & this.shownCastModeValue_) ||
            this.sinkToRouteMap_[element.id];
      }, this);
    }

    // When there's an updated list of sinks, append any new sinks to the end
    // of the existing list. This prevents sinks randomly jumping around the
    // dialog, which can surprise users / lead to inadvertently casting to the
    // wrong sink.
    if (this.sinksToShow_) {
      for (var i = this.sinksToShow_.length - 1; i >= 0; i--) {
        var index = updatedSinkList.findIndex(function(updatedSink) {
          return this.sinksToShow_[i].id == updatedSink.id;
        }.bind(this));
        if (index < 0) {
          // Remove any sinks that are no longer discovered.
          this.sinksToShow_.splice(i, 1);
        } else {
          // If the sink exists, move it from |updatedSinkList| to
          // |sinksToShow_| in the same position, as the cast modes or other
          // fields may have been updated.
          this.sinksToShow_[i] = updatedSinkList[index];
          updatedSinkList.splice(index, 1);
        }
      }

      updatedSinkList = this.sinksToShow_.concat(updatedSinkList);
    }
    this.sinksToShow_ = updatedSinkList;
  },

  /**
   * Called when |allSinks| is updated.
   *
   * @private
   */
  reindexSinksAndRebuildSinksToShow_: function() {
    this.sinkMap_ = {};

    this.allSinks.forEach(function(sink) {
      if (!sink.isPseudoSink) {
        this.sinkMap_[sink.id] = sink;
      }
    }, this);

    if (this.pseudoSinkSearchState_) {
      this.currentLaunchingSinkId_ =
          this.pseudoSinkSearchState_.checkForRealSink(this.allSinks);
    }
    this.pseudoSinks_ = this.allSinks.filter(function(sink) {
      return sink.isPseudoSink && !!sink.domain;
    });
    this.rebuildSinksToShow_();
    this.searchEnabled_ = this.searchEnabled_ || this.pseudoSinks_.length > 0 ||
        this.sinksToShow_.length >= media_router.MINIMUM_SINKS_FOR_SEARCH;
    this.filterSinks_(this.searchInputText_ || '');
    if (this.currentView_ != media_router.MediaRouterView.FILTER) {
      // This code is in the unique position of seeing |animationPromise_| as
      // null on startup. |allSinks| is initialized before |animationPromise_|
      // and this listener runs when |allSinks| is initialized.
      if (this.animationPromise_) {
        this.animationPromise_ =
            this.animationPromise_.then(this.putSearchAtBottom_.bind(this));
      } else {
        this.putSearchAtBottom_();
      }
    } else {
      this.queuePutSearchAtTop_();
    }
  },

  /**
   * Resets the properties relevant to creating a new route. Fires an event
   * indicating whether or not route creation was successful.
   * Clearing |currentLaunchingSinkId_| hides the spinner indicating there is
   * a route creation in progress and show the device icon instead.
   * @param {boolean} creationSuccess Whether route creation succeeded.
   *
   * @private
   */
  resetRouteCreationProperties_: function(creationSuccess) {
    this.pseudoSinkSearchState_ = null;
    this.currentLaunchingSinkId_ = '';
    this.pendingCreatedRouteId_ = '';
    // If it was a search that failed we need to refresh the filtered sinks now
    // that |pseudoSinkSearchState_| is null.
    if (!creationSuccess &&
        this.currentView_ == media_router.MediaRouterView.FILTER) {
      this.filterSinks_(this.searchInputText_);
    }

    this.fire('report-route-creation', {success: creationSuccess});
  },

  /**
   * Responds to a click on the search button by toggling sink filtering.
   */
  searchButtonClick_: function() {
    // Redundancy needed because focus() only fires event if input is not
    // already focused. In the case that user typed text, hit escape, then
    // clicks the search button, a focus event will not fire and so its event
    // handler from ready() will not run.
    this.showSearchResults_();
    this.$$('#sink-search-input').focus();
  },

  /**
   * Initializes the position of the search input if search becomes enabled.
   * @param {boolean} searchEnabled The new value of |searchEnabled_|.
   * @private
   */
  searchEnabledChanged_: function(searchEnabled) {
    if (searchEnabled) {
      this.async(function() {
        this.setSearchFocusHandlers_();
        this.putSearchAtBottom_();
      });
    }
  },

  /**
   * Filters the sink list when the input text changes and shows the search
   * results if |searchInputText| is not empty.
   * @param {string} searchInputText The currently entered search text.
   * @private
   */
  searchInputTextChanged_: function(searchInputText) {
    this.filterSinks_(searchInputText);
    if (searchInputText.length != 0) {
      this.showSearchResults_();
      this.maybeReportFilter_();
    }
  },

  /**
   * Sets the selected cast mode to the one associated with |castModeType|,
   * and rebuilds sinks to reflect the change.
   * @param {number} castModeType The type of the selected cast mode.
   */
  selectCastMode: function(castModeType) {
    var castMode = this.findCastModeByType_(castModeType);
    if (castMode && castModeType != this.shownCastModeValue_) {
      this.setShownCastMode_(castMode);
      this.userHasSelectedCastMode_ = true;
      this.rebuildSinksToShow_();
    }
  },

  /**
   * Fires the command to open a file dialog.
   *
   * @private
   */
  selectLocalMediaFile_() {
    this.fire('select-local-media-file');
  },

  /**
   * Sets various focus and blur event handlers to handle showing search results
   * when the search input is focused.
   * @private
   */
  setSearchFocusHandlers_: function() {
    var searchInput = this.$$('#sink-search-input');
    var that = this;

    // The window can see a blur event for two important cases: the window is
    // actually losing focus or keyboard focus is wrapping from the end of the
    // document to the beginning. To handle both cases, we save whether the
    // search input was focused during the window blur event.
    //
    // When the search input receives focus, it could be as part of window
    // focus. If the search input was also focused on window blur, it shouldn't
    // show search results if they aren't already being shown. Otherwise,
    // focusing the search input should activate the FILTER view by calling
    // |showSearchResults_()|.
    window.addEventListener('blur', function() {
      that.isSearchFocusedOnWindowBlur_ =
          that.shadowRoot.activeElement == searchInput;
    });
    searchInput.addEventListener('focus', function() {
      if (!that.isSearchFocusedOnWindowBlur_) {
        that.showSearchResults_();
      }
    });
  },

  /**
   * Updates the shown cast mode, and updates the header text fields
   * according to the cast mode. If |castMode| type is AUTO, then set
   * |userHasSelectedCastMode_| to false.
   *
   * @param {!media_router.CastMode} castMode
   */
  setShownCastMode_: function(castMode) {
    if (this.shownCastModeValue_ == castMode.type)
      return;

    this.shownCastModeValue_ = castMode.type;
    this.headerText = castMode.description;
    this.headerTextTooltip = castMode.host || '';
    if (castMode.type == media_router.CastModeType.AUTO)
      this.userHasSelectedCastMode_ = false;
  },

  /**
   * Shows the cast mode list.
   *
   * @private
   */
  showCastModeList_: function() {
    this.currentView_ = media_router.MediaRouterView.CAST_MODE_LIST;
  },

  /**
   * Creates a new route if there is no route to the |sink| . Otherwise,
   * shows the route details.
   *
   * @param {!media_router.Sink} sink The sink to use.
   * @private
   */
  showOrCreateRoute_: function(sink) {
    var route = this.sinkToRouteMap_[sink.id];
    if (route) {
      this.showRouteDetails_(route);
      this.fire('navigate-sink-list-to-details');
      this.maybeReportUserFirstAction(
          media_router.MediaRouterUserAction.STATUS_REMOTE);
    } else if (this.currentLaunchingSinkId_ == '') {
      // Allow one launch at a time.
      var selectedCastModeValue =
          this.shownCastModeValue_ == media_router.CastModeType.AUTO ?
          sink.castModes & -sink.castModes :
          this.shownCastModeValue_;
      if (sink.isPseudoSink) {
        this.pseudoSinkSearchState_ = new PseudoSinkSearchState(sink);
        this.fire('search-sinks-and-create-route', {
          id: sink.id,
          name: sink.name,
          domain: sink.domain,
          selectedCastMode: selectedCastModeValue
        });
      } else {
        this.fire('create-route', {
          sinkId: sink.id,
          // If user selected a cast mode, then we will create a route using
          // that cast mode. Otherwise, the UI is in "auto" cast mode and will
          // use the preferred cast mode compatible with the sink. The preferred
          // cast mode value is the least significant bit on the bitset.
          selectedCastModeValue: selectedCastModeValue
        });

        var timeToSelectSink =
            window.performance.now() - this.populatedSinkListSeenTimeMs_;
        this.fire('report-sink-click-time', {timeMs: timeToSelectSink});
      }
      this.currentLaunchingSinkId_ = sink.id;
      if (sink.isPseudoSink) {
        this.rebuildSinksToShow_();
      }

      this.maybeReportUserFirstAction(
          media_router.MediaRouterUserAction.START_LOCAL);
    }
  },

  /**
   * Shows the route details.
   *
   * @param {!media_router.Route} route The route to show.
   * @private
   */
  showRouteDetails_: function(route) {
    this.currentRoute_ = route;
    this.currentView_ = media_router.MediaRouterView.ROUTE_DETAILS;
    if (route.supportsWebUiController) {
      media_router.browserApi.onMediaControllerAvailable(route.id);
    }
    if (this.$$('route-details')) {
      this.$$('route-details').onOpened();
    }
  },

  /**
   * Shows the search results.
   *
   * @private
   */
  showSearchResults_: function() {
    if (this.currentView_ != media_router.MediaRouterView.FILTER) {
      this.currentView_ = media_router.MediaRouterView.FILTER;
      this.queueMoveSearchToTop_();
    }
  },

  /**
   * Shows the sink list.
   *
   * @private
   */
  showSinkList_: function() {
    if (this.currentView_ == media_router.MediaRouterView.FILTER) {
      this.queueMoveSearchToBottom_();
      this.currentView_ = media_router.MediaRouterView.SINK_LIST;
    } else {
      this.currentView_ = media_router.MediaRouterView.SINK_LIST;
      this.putSearchAtBottom_();
    }
  },

  /**
   * Starts a timer which fires a close-dialog event if the user's mouse is
   * not positioned over the dialog after three seconds.
   *
   * @private
   */
  startTapTimer_: function() {
    var id = setTimeout(function() {
      if (!this.mouseIsPositionedOverDialog_)
        this.fire('close-dialog', {
          pressEscToClose: false,
        });
    }.bind(this), 3000 /* 3 seconds */);
  },

  /**
   * Toggles |currentView_| between CAST_MODE_LIST and SINK_LIST.
   *
   * @private
   */
  toggleCastModeHidden_: function() {
    if (this.currentView_ == media_router.MediaRouterView.CAST_MODE_LIST) {
      this.showSinkList_();
    } else if (this.currentView_ == media_router.MediaRouterView.SINK_LIST) {
      this.showCastModeList_();
      this.fire('navigate-to-cast-mode-list');
    }
  },

  /**
   * Update the position-related styling of some elements.
   *
   * @private
   */
  updateElementPositioning_: function() {
    // Ensures that conditionally templated elements have finished stamping.
    this.async(function() {
      var headerHeight = this.header.offsetHeight;
      // Unlike the other elements whose heights are fixed, the first-run-flow
      // element can have a fractional height. So we use getBoundingClientRect()
      // to avoid rounding errors.
      var firstRunFlowHeight = this.$$('#first-run-flow') &&
              this.$$('#first-run-flow').style.display != 'none' ?
          this.$$('#first-run-flow').getBoundingClientRect().height :
          0;
      var issueHeight = this.$$('#issue-banner') &&
              this.$$('#issue-banner').style.display != 'none' ?
          this.$$('#issue-banner').offsetHeight :
          0;
      var search = this.$$('#sink-search');
      var hasSearch = this.hasConditionalElement_(search);
      var searchHeight = hasSearch ? search.offsetHeight : 0;
      var searchPadding =
          hasSearch ? this.computeElementVerticalPadding_(search) : 0;

      this.header.style.marginTop = firstRunFlowHeight + 'px';
      this.$['content'].style.marginTop =
          firstRunFlowHeight + headerHeight + 'px';

      var sinkList = this.$$('#sink-list');
      var sinkListPadding =
          sinkList ? this.computeElementVerticalPadding_(sinkList) : 0;

      this.sinkListMaxHeight_ = this.dialogHeight_ - headerHeight -
          firstRunFlowHeight - issueHeight - searchHeight + searchPadding -
          sinkListPadding;

      // Limit the height of the dialog to ten items, including search.
      var sinkItemHeight = 41;
      var maxSinkItems = hasSearch ? 9 : 10;
      this.sinkListMaxHeight_ =
          Math.min(sinkItemHeight * maxSinkItems, this.sinkListMaxHeight_);
      if (sinkList)
        sinkList.style.maxHeight = this.sinkListMaxHeight_ + 'px';
    });
  },

  /**
   * Update the max dialog height and update the positioning of the elements.
   *
   * @param {number} height The max height of the Media Router dialog.
   */
  updateMaxDialogHeight: function(height) {
    this.dialogHeight_ = height;
    this.updateElementPositioning_();
  },

  /**
   * Sets the selected cast mode menu item to be in sync with the current cast
   * mode.
   * @private
   */
  updateSelectedCastModeMenuItem_: function() {
    /** @const */ var curIndex =
        this.findCastModeIndexByType_(this.shownCastModeValue_);
    if (this.selectedCastModeMenuItem_ != curIndex)
      this.selectedCastModeMenuItem_ = curIndex;
  },
});

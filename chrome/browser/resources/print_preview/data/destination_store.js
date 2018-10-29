// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview');

/**
 * Printer search statuses used by the destination store.
 * @enum {string}
 */
print_preview.DestinationStorePrinterSearchStatus = {
  START: 'start',
  SEARCHING: 'searching',
  DONE: 'done'
};

cr.define('print_preview', function() {
  'use strict';
  /**
   * Localizes printer capabilities.
   * @param {!print_preview.Cdd} capabilities Printer capabilities to
   *     localize.
   * @return {!print_preview.Cdd} Localized capabilities.
   */
  const localizeCapabilities = function(capabilities) {
    if (!capabilities.printer)
      return capabilities;

    const mediaSize = capabilities.printer.media_size;
    if (!mediaSize)
      return capabilities;

    for (let i = 0, media; (media = mediaSize.option[i]); i++) {
      // No need to patch capabilities with localized names provided.
      if (!media.custom_display_name_localized) {
        media.custom_display_name = media.custom_display_name ||
            DestinationStore.MEDIA_DISPLAY_NAMES_[media.name] || media.name;
      }
    }
    return capabilities;
  };

  /**
   * Compare two media sizes by their names.
   * @param {!Object} a Media to compare.
   * @param {!Object} b Media to compare.
   * @return {number} 1 if a > b, -1 if a < b, or 0 if a == b.
   */
  const compareMediaNames = function(a, b) {
    const nameA = a.custom_display_name_localized || a.custom_display_name;
    const nameB = b.custom_display_name_localized || b.custom_display_name;
    return nameA == nameB ? 0 : (nameA > nameB ? 1 : -1);
  };

  /**
   * Sort printer media sizes.
   * @param {!print_preview.Cdd} capabilities Printer capabilities to
   * localize.
   * @return {!print_preview.Cdd} Localized capabilities.
   * @private
   */
  const sortMediaSizes = function(capabilities) {
    if (!capabilities.printer)
      return capabilities;

    const mediaSize = capabilities.printer.media_size;
    if (!mediaSize)
      return capabilities;

    // For the standard sizes, separate into categories, as seen in the Cloud
    // Print CDD guide:
    // - North American
    // - Chinese
    // - ISO
    // - Japanese
    // - Other metric
    // Otherwise, assume they are custom sizes.
    const categoryStandardNA = [];
    const categoryStandardCN = [];
    const categoryStandardISO = [];
    const categoryStandardJP = [];
    const categoryStandardMisc = [];
    const categoryCustom = [];
    for (let i = 0, media; (media = mediaSize.option[i]); i++) {
      const name = media.name || 'CUSTOM';
      let category;
      if (name.startsWith('NA_')) {
        category = categoryStandardNA;
      } else if (
          name.startsWith('PRC_') || name.startsWith('ROC_') ||
          name == 'OM_DAI_PA_KAI' || name == 'OM_JUURO_KU_KAI' ||
          name == 'OM_PA_KAI') {
        category = categoryStandardCN;
      } else if (name.startsWith('ISO_')) {
        category = categoryStandardISO;
      } else if (name.startsWith('JIS_') || name.startsWith('JPN_')) {
        category = categoryStandardJP;
      } else if (name.startsWith('OM_')) {
        category = categoryStandardMisc;
      } else {
        assert(name == 'CUSTOM', 'Unknown media size. Assuming custom');
        category = categoryCustom;
      }
      category.push(media);
    }

    // For each category, sort by name.
    categoryStandardNA.sort(compareMediaNames);
    categoryStandardCN.sort(compareMediaNames);
    categoryStandardISO.sort(compareMediaNames);
    categoryStandardJP.sort(compareMediaNames);
    categoryStandardMisc.sort(compareMediaNames);
    categoryCustom.sort(compareMediaNames);

    // Then put it all back together.
    mediaSize.option = categoryStandardNA;
    mediaSize.option.push(
        ...categoryStandardCN, ...categoryStandardISO, ...categoryStandardJP,
        ...categoryStandardMisc, ...categoryCustom);
    return capabilities;
  };


  class DestinationStore extends cr.EventTarget {
    /**
     * A data store that stores destinations and dispatches events when the
     * data store changes.
     * @param {!print_preview.UserInfo} userInfo User information repository.
     * @param {!WebUIListenerTracker} listenerTracker Tracker for WebUI
     *     listeners added in DestinationStore constructor.
     */
    constructor(userInfo, listenerTracker) {
      super();

      /**
       * Used to fetch local print destinations.
       * @private {!print_preview.NativeLayer}
       */
      this.nativeLayer_ = print_preview.NativeLayer.getInstance();

      /**
       * User information repository.
       * @private {!print_preview.UserInfo}
       */
      this.userInfo_ = userInfo;

      /**
       * Used to track metrics.
       * @private {!print_preview.DestinationSearchMetricsContext}
       */
      this.metrics_ = new print_preview.DestinationSearchMetricsContext();

      /**
       * Internal backing store for the data store.
       * @private {!Array<!print_preview.Destination>}
       */
      this.destinations_ = [];

      /**
       * Cache used for constant lookup of destinations by origin and id.
       * @private {Object<!print_preview.Destination>}
       */
      this.destinationMap_ = {};

      /**
       * Currently selected destination.
       * @private {print_preview.Destination}
       */
      this.selectedDestination_ = null;

      /**
       * Whether the destination store will auto select the destination that
       * matches this set of parameters.
       * @private {print_preview.DestinationMatch}
       */
      this.autoSelectMatchingDestination_ = null;

      /**
       * Event tracker used to track event listeners of the destination store.
       * @private {!EventTracker}
       */
      this.tracker_ = new EventTracker();

      /**
       * Whether PDF printer is enabled. It's disabled, for example, in App
       * Kiosk mode.
       * @private {boolean}
       */
      this.pdfPrinterEnabled_ = false;

      /**
       * ID of the system default destination.
       * @private {string}
       */
      this.systemDefaultDestinationId_ = '';

      /**
       * Used to fetch cloud-based print destinations.
       * @private {cloudprint.CloudPrintInterface}
       */
      this.cloudPrintInterface_ = null;

      /**
       * Maps user account to the list of origins for which destinations are
       * already loaded.
       * @private {!Object<Array<!print_preview.DestinationOrigin>>}
       */
      this.loadedCloudOrigins_ = {};

      /**
       * ID of a timeout after the initial destination ID is set. If no inserted
       * destination matches the initial destination ID after the specified
       * timeout, the first destination in the store will be automatically
       * selected.
       * @private {?number}
       */
      this.autoSelectTimeout_ = null;

      /**
       * Whether a search for destinations is in progress for each type of
       * printer.
       * @private {!Map<!print_preview.PrinterType,
       *                !print_preview.DestinationStorePrinterSearchStatus>}
       */
      this.destinationSearchStatus_ = new Map([
        [
          print_preview.PrinterType.EXTENSION_PRINTER,
          print_preview.DestinationStorePrinterSearchStatus.START
        ],
        [
          print_preview.PrinterType.PRIVET_PRINTER,
          print_preview.DestinationStorePrinterSearchStatus.START
        ],
        [
          print_preview.PrinterType.LOCAL_PRINTER,
          print_preview.DestinationStorePrinterSearchStatus.START
        ],
        [
          print_preview.PrinterType.CLOUD_PRINTER,
          print_preview.DestinationStorePrinterSearchStatus.START
        ]
      ]);

      /**
       * MDNS service name of destination that we are waiting to register.
       * @private {?string}
       */
      this.waitForRegisterDestination_ = null;

      /**
       * Local destinations are CROS destinations on ChromeOS because they
       * require extra setup.
       * @private {!print_preview.DestinationOrigin}
       */
      this.platformOrigin_ = cr.isChromeOS ?
          print_preview.DestinationOrigin.CROS :
          print_preview.DestinationOrigin.LOCAL;

      /**
       * Whether to default to the system default printer instead of the most
       * recent destination.
       * @private {boolean}
       */
      this.useSystemDefaultAsDefault_ =
          loadTimeData.getBoolean('useSystemDefaultPrinter');

      /**
       * The recent print destinations, set when the store is initialized.
       * @private {!Array<!print_preview.RecentDestination>}
       */
      this.recentDestinations_ = [];

      this.reset_();

      this.addWebUIEventListeners_(listenerTracker);
    }

    /**
     * @param {?string=} opt_account Account to filter destinations by. When
     *     null or omitted, all destinations are returned.
     * @return {!Array<!print_preview.Destination>} List of destinations
     *     accessible by the {@code account}.
     */
    destinations(opt_account) {
      if (opt_account) {
        return this.destinations_.filter(function(destination) {
          return !destination.account || destination.account == opt_account;
        });
      }
      return this.destinations_.slice(0);
    }

    /**
     * Gets the destination, if any, matching |account|, |id|, and |origin| in
     * the destination map.
     * @param {!print_preview.DestinationOrigin} origin The origin of the
     *     destination.
     * @param {string} id The destination ID
     * @param {string} account The account the destination is associated with.
     * @return {?print_preview.Destination}
     */
    getDestination(origin, id, account) {
      return this.destinationMap_[this.getDestinationKey_(origin, id, account)];
    }

    /**
     * @return {print_preview.Destination} The currently selected destination or
     *     {@code null} if none is selected.
     */
    get selectedDestination() {
      return this.selectedDestination_;
    }

    /** @return {boolean} Whether destination selection is pending or not. */
    get isAutoSelectDestinationInProgress() {
      return this.selectedDestination_ == null &&
          this.autoSelectTimeout_ != null;
    }

    /**
     * @return {boolean} Whether a search for print destinations is in progress.
     */
    get isPrintDestinationSearchInProgress() {
      let isLocalDestinationSearchInProgress =
          Array.from(this.destinationSearchStatus_.values())
              .some(
                  el => el ===
                      print_preview.DestinationStorePrinterSearchStatus
                          .SEARCHING);
      if (isLocalDestinationSearchInProgress)
        return true;

      let isCloudDestinationSearchInProgress = !!this.cloudPrintInterface_ &&
          this.cloudPrintInterface_.isCloudDestinationSearchInProgress;
      return isCloudDestinationSearchInProgress;
    }

    /**
     * Starts listening for relevant WebUI events and adds the listeners to
     * |listenerTracker|. |listenerTracker| is responsible for removing the
     * listeners when necessary.
     * @param {!WebUIListenerTracker} listenerTracker
     * @private
     */
    addWebUIEventListeners_(listenerTracker) {
      listenerTracker.add('printers-added', this.onPrintersAdded_.bind(this));
      listenerTracker.add(
          'reload-printer-list', this.onDestinationsReload.bind(this));
    }

    /**
     * @param {(?print_preview.Destination |
     *          ?print_preview.RecentDestination)} destination
     * @return {boolean} Whether the destination is valid.
     */
    isDestinationValid(destination) {
      return !!destination && !!destination.id && !!destination.origin;
    }

    /**
     * Initializes the destination store. Sets the initially selected
     * destination. If any inserted destinations match this ID, that destination
     * will be automatically selected.
     * @param {boolean} isInAppKioskMode Whether the print preview is in App
     *     Kiosk mode.
     * @param {string} systemDefaultDestinationId ID of the system default
     *     destination.
     * @param {?string} serializedDefaultDestinationSelectionRulesStr Serialized
     *     default destination selection rules.
     * @param {!Array<!print_preview.RecentDestination>}
     *     recentDestinations The recent print destinations.
     */
    init(
        isInAppKioskMode, systemDefaultDestinationId,
        serializedDefaultDestinationSelectionRulesStr, recentDestinations) {
      this.pdfPrinterEnabled_ = !isInAppKioskMode;
      this.systemDefaultDestinationId_ = systemDefaultDestinationId;
      this.createLocalPdfPrintDestination_();

      const isRecentDestinationValid = recentDestinations.length > 0 &&
          this.isDestinationValid(recentDestinations[0]);

      if (!isRecentDestinationValid) {
        const destinationMatch = this.convertToDestinationMatch_(
            serializedDefaultDestinationSelectionRulesStr);
        if (destinationMatch) {
          this.fetchMatchingDestination_(destinationMatch);
          return;
        }
      }

      if (this.systemDefaultDestinationId_.length == 0 &&
          !isRecentDestinationValid) {
        this.selectPdfDestination_();
        return;
      }

      this.recentDestinations_ = recentDestinations;
      let origin = null;
      let id = '';
      let account = '';
      let name = '';
      let capabilities = null;
      let extensionId = '';
      let extensionName = '';
      let foundDestination = false;
      // Run through the destinations forward. As soon as we find a
      // destination, don't select any future destinations, just mark
      // them recent. Otherwise, there is a race condition between selecting
      // destinations/updating the print ticket and this selecting a new
      // destination that causes random print preview errors.
      for (let destination of recentDestinations) {
        origin = destination.origin;
        id = destination.id;
        account = destination.account || '';
        name = destination.displayName || '';
        capabilities = destination.capabilities;
        extensionId = destination.extensionId || '';
        extensionName = destination.extensionName || '';
        const candidate =
            this.destinationMap_[this.getDestinationKey_(origin, id, account)];
        if (candidate != null) {
          candidate.isRecent = true;
          if (!foundDestination && !this.useSystemDefaultAsDefault_)
            this.selectDestination(candidate);
          foundDestination = true;
        } else if (!foundDestination && !this.useSystemDefaultAsDefault_) {
          foundDestination = this.fetchPreselectedDestination_(
              origin, id, account, name, capabilities, extensionId,
              extensionName);
        }
      }

      if (foundDestination && !this.useSystemDefaultAsDefault_)
        return;

      // Try the system default
      id = this.systemDefaultDestinationId_;
      origin = id == print_preview.Destination.GooglePromotedId.SAVE_AS_PDF ?
          print_preview.DestinationOrigin.LOCAL :
          this.platformOrigin_;
      account = '';
      const systemDefaultCandidate =
          this.destinationMap_[this.getDestinationKey_(origin, id, account)];
      if (systemDefaultCandidate != null) {
        this.selectDestination(systemDefaultCandidate);
        return;
      }

      if (this.fetchPreselectedDestination_(
              origin, id, account, name, capabilities, extensionId,
              extensionName)) {
        return;
      }

      this.selectPdfDestination_();
    }

    /**
     * Attempts to fetch capabilities of the destination identified by the
     * provided origin, id and account.
     * @param {print_preview.DestinationOrigin} origin Destination
     *     origin.
     * @param {string} id Destination id.
     * @param {string} account User account destination is registered for.
     * @param {string} name Destination display name.
     * @param {?print_preview.Cdd} capabilities Destination capabilities.
     * @param {string} extensionId Extension ID associated with this
     *     destination.
     * @param {string} extensionName Extension name associated with this
     *     destination.
     * @return {boolean} Whether capabilities fetch was successfully started.
     * @private
     */
    fetchPreselectedDestination_(
        origin, id, account, name, capabilities, extensionId, extensionName) {
      this.autoSelectMatchingDestination_ =
          this.createExactDestinationMatch_(origin, id);

      const type = print_preview.originToType(origin);
      if (type == print_preview.PrinterType.LOCAL_PRINTER) {
        this.nativeLayer_.getPrinterCapabilities(id, type).then(
            this.onCapabilitiesSet_.bind(this, origin, id),
            this.onGetCapabilitiesFail_.bind(this, origin, id));
        return true;
      }

      if (this.cloudPrintInterface_ &&
          (origin == print_preview.DestinationOrigin.COOKIES ||
           origin == print_preview.DestinationOrigin.DEVICE)) {
        this.cloudPrintInterface_.printer(id, origin, account);
        return true;
      }

      if (origin == print_preview.DestinationOrigin.PRIVET ||
          origin == print_preview.DestinationOrigin.EXTENSION) {
        // TODO(noamsml): Resolve a specific printer instead of listing all
        // privet or extension printers in this case.
        this.startLoadDestinations(type);

        // Create a fake selectedDestination_ that is not actually in the
        // destination store. When the real destination is created, this
        // destination will be overwritten.
        const params =
            (origin === print_preview.DestinationOrigin.PRIVET) ? {} : {
              description: '',
              extensionId: extensionId,
              extensionName: extensionName,
              provisionalType: print_preview.DestinationProvisionalType.NONE
            };
        this.selectedDestination_ = new print_preview.Destination(
            id, print_preview.DestinationType.LOCAL, origin, name,
            false /*isRecent*/,
            print_preview.DestinationConnectionStatus.ONLINE, params);

        if (capabilities) {
          this.selectedDestination_.capabilities = capabilities;

          cr.dispatchSimpleEvent(
              this,
              DestinationStore.EventType
                  .CACHED_SELECTED_DESTINATION_INFO_READY);
        }
        return true;
      }
      return false;
    }

    /**
     * Attempts to find a destination matching the provided rules.
     * @param {!print_preview.DestinationMatch} destinationMatch Rules to match.
     * @private
     */
    fetchMatchingDestination_(destinationMatch) {
      this.autoSelectMatchingDestination_ = destinationMatch;
      const types = destinationMatch.getTypes();
      types.forEach(type => {
        if (type != null) {  // Local, extension, or privet printer
          this.startLoadDestinations(type);
        } else if (
            destinationMatch.matchOrigin(
                print_preview.DestinationOrigin.COOKIES) ||
            destinationMatch.matchOrigin(
                print_preview.DestinationOrigin.DEVICE)) {
          this.startLoadCloudDestinations();
        }
      });
    }

    /**
     * @param {?string} serializedDefaultDestinationSelectionRulesStr Serialized
     *     default destination selection rules.
     * @return {?print_preview.DestinationMatch} Creates rules matching
     *     previously selected destination.
     * @private
     */
    convertToDestinationMatch_(serializedDefaultDestinationSelectionRulesStr) {
      let matchRules = null;
      try {
        if (serializedDefaultDestinationSelectionRulesStr) {
          matchRules =
              JSON.parse(serializedDefaultDestinationSelectionRulesStr);
        }
      } catch (e) {
        console.error('Failed to parse defaultDestinationSelectionRules: ' + e);
      }
      if (!matchRules)
        return null;

      const isLocal = !matchRules.kind || matchRules.kind == 'local';
      const isCloud = !matchRules.kind || matchRules.kind == 'cloud';
      if (!isLocal && !isCloud) {
        console.error('Unsupported type: "' + matchRules.kind + '"');
        return null;
      }

      const origins = [];
      if (isLocal) {
        origins.push(print_preview.DestinationOrigin.LOCAL);
        origins.push(print_preview.DestinationOrigin.PRIVET);
        origins.push(print_preview.DestinationOrigin.EXTENSION);
        origins.push(print_preview.DestinationOrigin.CROS);
      }
      if (isCloud) {
        origins.push(print_preview.DestinationOrigin.COOKIES);
        origins.push(print_preview.DestinationOrigin.DEVICE);
      }

      let idRegExp = null;
      try {
        if (matchRules.idPattern) {
          idRegExp = new RegExp(matchRules.idPattern || '.*');
        }
      } catch (e) {
        console.error('Failed to parse regexp for "id": ' + e);
      }

      let displayNameRegExp = null;
      try {
        if (matchRules.namePattern) {
          displayNameRegExp = new RegExp(matchRules.namePattern || '.*');
        }
      } catch (e) {
        console.error('Failed to parse regexp for "name": ' + e);
      }

      return new print_preview.DestinationMatch(
          origins, idRegExp, displayNameRegExp,
          true /*skipVirtualDestinations*/);
    }

    /**
     * @return {print_preview.DestinationMatch} Creates rules matching
     *     previously selected destination.
     * @private
     */
    convertPreselectedToDestinationMatch_() {
      if (this.isDestinationValid(this.selectedDestination_)) {
        return this.createExactDestinationMatch_(
            this.selectedDestination_.origin, this.selectedDestination_.id);
      }
      if (this.systemDefaultDestinationId_.length > 0) {
        return this.createExactDestinationMatch_(
            this.platformOrigin_, this.systemDefaultDestinationId_);
      }
      return null;
    }

    /**
     * @param {string | print_preview.DestinationOrigin} origin Destination
     *     origin.
     * @param {string} id Destination id.
     * @return {!print_preview.DestinationMatch} Creates rules matching
     *     provided destination.
     * @private
     */
    createExactDestinationMatch_(origin, id) {
      return new print_preview.DestinationMatch(
          [origin],
          new RegExp('^' + id.replace(/[.*+?^${}()|[\]\\]/g, '\\$&') + '$'),
          null /*displayNameRegExp*/, false /*skipVirtualDestinations*/);
    }

    /**
     * Sets the destination store's Google Cloud Print interface.
     * @param {!cloudprint.CloudPrintInterface} cloudPrintInterface Interface
     *     to set.
     */
    setCloudPrintInterface(cloudPrintInterface) {
      assert(this.cloudPrintInterface_ == null);
      this.cloudPrintInterface_ = cloudPrintInterface;
      this.tracker_.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterfaceEventType.SEARCH_DONE,
          this.onCloudPrintSearchDone_.bind(this));
      this.tracker_.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterfaceEventType.SEARCH_FAILED,
          this.onCloudPrintSearchDone_.bind(this));
      this.tracker_.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterfaceEventType.PRINTER_DONE,
          this.onCloudPrintPrinterDone_.bind(this));
      this.tracker_.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterfaceEventType.PRINTER_FAILED,
          this.onCloudPrintPrinterFailed_.bind(this));
      this.tracker_.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterfaceEventType.PROCESS_INVITE_DONE,
          this.onCloudPrintProcessInviteDone_.bind(this));
    }

    /**
     * @param {print_preview.Destination} destination Destination to select.
     */
    selectDestination(destination) {
      this.autoSelectMatchingDestination_ = null;
      // When auto select expires, DESTINATION_SELECT event has to be dispatched
      // anyway (see isAutoSelectDestinationInProgress() logic).
      if (this.autoSelectTimeout_) {
        clearTimeout(this.autoSelectTimeout_);
        this.autoSelectTimeout_ = null;
      } else if (destination == this.selectedDestination_) {
        return;
      }
      if (destination == null) {
        this.selectedDestination_ = null;
        cr.dispatchSimpleEvent(
            this, DestinationStore.EventType.DESTINATION_SELECT);
        return;
      }

      assert(
          !destination.isProvisional,
          'Unable to select provisonal destinations');

      // Update and persist selected destination.
      this.selectedDestination_ = destination;
      this.selectedDestination_.isRecent = true;
      // Adjust metrics.
      if (destination.cloudID &&
          this.destinations_.some(function(otherDestination) {
            return otherDestination.cloudID == destination.cloudID &&
                otherDestination != destination;
          })) {
        this.metrics_.record(
            destination.isPrivet ? print_preview.Metrics.DestinationSearchBucket
                                       .PRIVET_DUPLICATE_SELECTED :
                                   print_preview.Metrics.DestinationSearchBucket
                                       .CLOUD_DUPLICATE_SELECTED);
      }
      // Notify about selected destination change.
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATION_SELECT);
      // Request destination capabilities from backend, since they are not
      // known yet.
      if (destination.capabilities == null) {
        const type = print_preview.originToType(destination.origin);
        if (type !== null) {
          this.nativeLayer_.getPrinterCapabilities(destination.id, type)
              .then(
                  (caps) => this.onCapabilitiesSet_(
                      destination.origin, destination.id, caps),
                  () => this.onGetCapabilitiesFail_(
                      destination.origin, destination.id));
        } else {
          assert(
              this.cloudPrintInterface_ != null,
              'Cloud destination selected, but GCP is not enabled');
          this.cloudPrintInterface_.printer(
              destination.id, destination.origin, destination.account);
        }
      } else {
        this.sendSelectedDestinationUpdateEvent_();
      }
    }

    /**
     * Attempt to resolve the capabilities for a Chrome OS printer.
     * @param {!print_preview.Destination} destination The destination which
     *     requires resolution.
     * @return {!Promise<!print_preview.PrinterSetupResponse>}
     */
    resolveCrosDestination(destination) {
      assert(destination.origin == print_preview.DestinationOrigin.CROS);
      return this.nativeLayer_.setupPrinter(destination.id);
    }

    /**
     * Attempts to resolve a provisional destination.
     * @param {!print_preview.Destination} destination Provisional destination
     *     that should be resolved.
     * @return {!Promise<?print_preview.Destination>}
     */
    resolveProvisionalDestination(destination) {
      assert(
          destination.provisionalType ==
              print_preview.DestinationProvisionalType.NEEDS_USB_PERMISSION,
          'Provisional type cannot be resolved.');
      return this.nativeLayer_.grantExtensionPrinterAccess(destination.id)
          .then(
              destinationInfo => {
                /**
                 * Removes the destination from the store and replaces it with a
                 * destination created from the resolved destination properties,
                 * if any are reported. Then sends a
                 * PROVISIONAL_DESTINATION_RESOLVED event.
                 */
                this.removeProvisionalDestination_(destination.id);
                const parsedDestination =
                    print_preview.parseExtensionDestination(destinationInfo);
                this.insertIntoStore_(parsedDestination);
                this.dispatchProvisionalDestinationResolvedEvent_(
                    destination.id, parsedDestination);
                return parsedDestination;
              },
              () => {
                /**
                 * The provisional destination is removed from the store and a
                 * PROVISIONAL_DESTINATION_RESOLVED event is dispatched with a
                 * null destination.
                 */
                this.removeProvisionalDestination_(destination.id);
                this.dispatchProvisionalDestinationResolvedEvent_(
                    destination.id, null);
                return null;
              });
    }

    /**
     * Selects 'Save to PDF' destination (since it always exists).
     * @private
     */
    selectPdfDestination_() {
      const saveToPdfKey = this.getDestinationKey_(
          print_preview.DestinationOrigin.LOCAL,
          print_preview.Destination.GooglePromotedId.SAVE_AS_PDF, '');
      this.selectDestination(
          this.destinationMap_[saveToPdfKey] || this.destinations_[0] || null);
    }

    /**
     * Attempts to select system default destination with a fallback to
     * 'Save to PDF' destination.
     * @private
     */
    selectDefaultDestination_() {
      if (this.systemDefaultDestinationId_.length > 0) {
        if (this.autoSelectMatchingDestination_ &&
            !this.autoSelectMatchingDestination_.matchIdAndOrigin(
                this.systemDefaultDestinationId_, this.platformOrigin_)) {
          if (this.fetchPreselectedDestination_(
                  this.platformOrigin_, this.systemDefaultDestinationId_,
                  '' /*account*/, '' /*name*/, null /*capabilities*/,
                  '' /*extensionId*/, '' /*extensionName*/)) {
            return;
          }
        }
      }
      this.selectPdfDestination_();
    }

    /**
     * Initiates loading of destinations.
     * @param{print_preview.PrinterType} type The type of destinations to load.
     */
    startLoadDestinations(type) {
      if (this.destinationSearchStatus_.get(type) ===
          print_preview.DestinationStorePrinterSearchStatus.DONE) {
        return;
      }
      this.destinationSearchStatus_.set(
          type, print_preview.DestinationStorePrinterSearchStatus.SEARCHING);
      this.nativeLayer_.getPrinters(type).then(
          this.onDestinationSearchDone_.bind(this, type), () => {
            // Will be rejected by C++ for privet printers if privet printing
            // is disabled.
            assert(type === print_preview.PrinterType.PRIVET_PRINTER);
            this.destinationSearchStatus_.set(
                type, print_preview.DestinationStorePrinterSearchStatus.DONE);
          });
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATION_SEARCH_STARTED);
    }

    /**
     * Initiates loading of cloud destinations.
     * @param {print_preview.DestinationOrigin=} opt_origin Search destinations
     *     for the specified origin only.
     */
    startLoadCloudDestinations(opt_origin) {
      if (this.cloudPrintInterface_ != null) {
        const origins =
            this.loadedCloudOrigins_[this.userInfo_.activeUser] || [];
        if (origins.length == 0 ||
            (opt_origin && origins.indexOf(opt_origin) < 0)) {
          this.cloudPrintInterface_.search(
              this.userInfo_.activeUser, opt_origin);
          cr.dispatchSimpleEvent(
              this, DestinationStore.EventType.DESTINATION_SEARCH_STARTED);
        }
      }
    }

    /** Requests load of COOKIE based cloud destinations. */
    reloadUserCookieBasedDestinations() {
      const origins = this.loadedCloudOrigins_[this.userInfo_.activeUser] || [];
      if (origins.indexOf(print_preview.DestinationOrigin.COOKIES) >= 0) {
        cr.dispatchSimpleEvent(
            this, DestinationStore.EventType.DESTINATION_SEARCH_DONE);
      } else {
        this.startLoadCloudDestinations(
            print_preview.DestinationOrigin.COOKIES);
      }
    }

    /** Initiates loading of all known destination types. */
    startLoadAllDestinations() {
      // Printer types that need to be retrieved from the handler.
      let types = [
        print_preview.PrinterType.PRIVET_PRINTER,
        print_preview.PrinterType.EXTENSION_PRINTER,
        print_preview.PrinterType.LOCAL_PRINTER,
      ];

      // If the cloud printer handler is enabled, request cloud printers from
      // the handler instead of trying to directly communicate with the cloud
      // print server. See https://crbug.com/829414.
      if (loadTimeData.getBoolean('cloudPrinterHandlerEnabled'))
        types.push(print_preview.PrinterType.CLOUD_PRINTER);
      else
        this.startLoadCloudDestinations();

      for (const printerType of types)
        this.startLoadDestinations(printerType);
    }

    /**
     * Wait for a privet device to be registered.
     */
    waitForRegister(id) {
      const privetType = print_preview.PrinterType.PRIVET_PRINTER;
      this.nativeLayer_.getPrinters(privetType)
          .then(this.onDestinationSearchDone_.bind(this, privetType));
      this.waitForRegisterDestination_ = id;
    }

    /**
     * Removes the provisional destination with ID |provisionalId| from
     * |destinationMap_| and |destinations_|.
     * @param{string} provisionalId The provisional destination ID.
     * @private
     */
    removeProvisionalDestination_(provisionalId) {
      this.destinations_ = this.destinations_.filter(
          function(el) {
            if (el.id == provisionalId) {
              delete this.destinationMap_[this.getKey_(el)];
              return false;
            }
            return true;
          }, this);
    }

    /**
     * Dispatches the PROVISIONAL_DESTINATION_RESOLVED event for id
     * |provisionalId| and destination |destination|.
     * @param {string} provisionalId The ID of the destination that was
     *     resolved.
     * @param {?print_preview.Destination} destination Information about the
     *     destination if it was resolved successfully.
     */
    dispatchProvisionalDestinationResolvedEvent_(provisionalId, destination) {
      const event = new Event(
          DestinationStore.EventType.PROVISIONAL_DESTINATION_RESOLVED);
      event.provisionalId = provisionalId;
      event.destination = destination;
      this.dispatchEvent(event);
    }

    /**
     * Inserts {@code destination} to the data store and dispatches a
     * DESTINATIONS_INSERTED event.
     * @param {!print_preview.Destination} destination Print destination to
     *     insert.
     * @private
     */
    insertDestination_(destination) {
      if (this.insertIntoStore_(destination)) {
        this.destinationsInserted_(destination);
      }
    }

    /**
     * Inserts multiple {@code destinations} to the data store and dispatches
     * single DESTINATIONS_INSERTED event.
     * @param {!Array<!print_preview.Destination |
     *                !Array<print_preview.Destination>>} destinations Print
     *     destinations to insert.
     * @private
     */
    insertDestinations_(destinations) {
      let inserted = false;
      destinations.forEach(destination => {
        if (Array.isArray(destination)) {
          // privet printers return arrays of 1 or 2 printers
          inserted = destination.reduce(function(soFar, d) {
            return this.insertIntoStore_(d) || soFar;
          }, inserted);
        } else {
          inserted = this.insertIntoStore_(destination) || inserted;
        }
      });
      if (inserted) {
        this.destinationsInserted_();
      }
    }

    /**
     * Dispatches DESTINATIONS_INSERTED event. In auto select mode, tries to
     * update selected destination to match
     * {@code autoSelectMatchingDestination_}.
     * @param {print_preview.Destination=} opt_destination The only destination
     *     that was changed or skipped if possibly more than one destination was
     *     changed. Used as a hint to limit destination search scope against
     *     {@code autoSelectMatchingDestination_}.
     */
    destinationsInserted_(opt_destination) {
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATIONS_INSERTED);
      if (this.autoSelectMatchingDestination_) {
        const destinationsToSearch =
            opt_destination && [opt_destination] || this.destinations_;
        destinationsToSearch.some(function(destination) {
          if (this.autoSelectMatchingDestination_.match(destination)) {
            this.selectDestination(destination);
            return true;
          }
        }, this);
      }
    }

    /**
     * Sends SELECTED_DESTINATION_CAPABILITIES_READY event if the destination
     * is supported, or SELECTED_DESTINATION_UNSUPPORTED otherwise.
     * @private
     */
    sendSelectedDestinationUpdateEvent_() {
      cr.dispatchSimpleEvent(
          this,
          this.selectedDestination_.shouldShowInvalidCertificateError ?
              DestinationStore.EventType.SELECTED_DESTINATION_UNSUPPORTED :
              DestinationStore.EventType
                  .SELECTED_DESTINATION_CAPABILITIES_READY);
    }

    /**
     * Updates an existing print destination with capabilities and display name
     * information. If the destination doesn't already exist, it will be added.
     * @param {!print_preview.Destination} destination Destination to update.
     * @private
     */
    updateDestination_(destination) {
      assert(destination.constructor !== Array, 'Single printer expected');
      destination.capabilities =
          localizeCapabilities(assert(destination.capabilities));
      if (print_preview.originToType(destination.origin) !==
          print_preview.PrinterType.LOCAL_PRINTER) {
        destination.capabilities = sortMediaSizes(destination.capabilities);
      }
      const existingDestination =
          this.destinationMap_[this.getKey_(destination)];
      if (existingDestination != null) {
        existingDestination.capabilities = destination.capabilities;
      } else {
        this.insertDestination_(destination);
      }

      if (this.selectedDestination_ &&
          (existingDestination == this.selectedDestination_ ||
           destination == this.selectedDestination_)) {
        this.sendSelectedDestinationUpdateEvent_();
      }
    }

    /**
     * Called when loading of extension managed printers is done.
     * @private
     */
    endExtensionPrinterSearch_() {
      // Clear initially selected (cached) extension destination if it hasn't
      // been found among reported extension destinations.
      if (this.autoSelectMatchingDestination_ &&
          this.autoSelectMatchingDestination_.matchOrigin(
              print_preview.DestinationOrigin.EXTENSION) &&
          this.selectedDestination_ && this.selectedDestination_.isExtension) {
        this.selectDefaultDestination_();
      }
    }

    /**
     * Inserts a destination into the store without dispatching any events.
     * @param {!print_preview.Destination} destination The destination to be
     *     inserted.
     * @return {boolean} Whether the inserted destination was not already in the
     *     store.
     * @private
     */
    insertIntoStore_(destination) {
      const key = this.getKey_(destination);
      const existingDestination = this.destinationMap_[key];
      if (existingDestination == null) {
        destination.isRecent = destination.isRecent ||
            this.recentDestinations_.some(function(recent) {
              return (
                  destination.id == recent.id &&
                  destination.origin == recent.origin);
            }, this);
        this.destinations_.push(destination);
        this.destinationMap_[key] = destination;
        return true;
      }
      if (existingDestination.connectionStatus ==
              print_preview.DestinationConnectionStatus.UNKNOWN &&
          destination.connectionStatus !=
              print_preview.DestinationConnectionStatus.UNKNOWN) {
        existingDestination.connectionStatus = destination.connectionStatus;
        return true;
      }
      return false;
    }

    /**
     * Creates a local PDF print destination.
     * @private
     */
    createLocalPdfPrintDestination_() {
      // TODO(alekseys): Create PDF printer in the native code and send its
      // capabilities back with other local printers.
      if (this.pdfPrinterEnabled_) {
        this.insertDestination_(new print_preview.Destination(
            print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
            print_preview.DestinationType.LOCAL,
            print_preview.DestinationOrigin.LOCAL,
            loadTimeData.getString('printToPDF'), false /*isRecent*/,
            print_preview.DestinationConnectionStatus.ONLINE));
      }
    }

    /**
     * Resets the state of the destination store to its initial state.
     * @private
     */
    reset_() {
      this.destinations_ = [];
      this.destinationMap_ = {};
      this.selectDestination(null);
      this.loadedCloudOrigins_ = {};
      for (const printerType of Object.values(print_preview.PrinterType)) {
        if (printerType !== print_preview.PrinterType.PDF_PRINTER) {
          this.destinationSearchStatus_.set(
              printerType,
              print_preview.DestinationStorePrinterSearchStatus.START);
        }
      }

      clearTimeout(this.autoSelectTimeout_);
      this.autoSelectTimeout_ = setTimeout(
          this.selectDefaultDestination_.bind(this),
          DestinationStore.AUTO_SELECT_TIMEOUT_);
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATIONS_RESET);
    }


    /**
     * Called when destination search is complete for some type of printer.
     * @param {!print_preview.PrinterType} type The type of printers that are
     *     done being retreived.
     */
    onDestinationSearchDone_(type) {
      this.destinationSearchStatus_.set(
          type, print_preview.DestinationStorePrinterSearchStatus.DONE);
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATION_SEARCH_DONE);
      if (type === print_preview.PrinterType.EXTENSION_PRINTER)
        this.endExtensionPrinterSearch_();
    }

    /**
     * Called when the native layer retrieves the capabilities for the selected
     * local destination. Updates the destination with new capabilities if the
     * destination already exists, otherwise it creates a new destination and
     * then updates its capabilities.
     * @param {!print_preview.DestinationOrigin} origin The origin of the
     *     print destination.
     * @param {string} id The id of the print destination.
     * @param {!print_preview.CapabilitiesResponse} settingsInfo Contains
     *     the capabilities of the print destination, and information about
     *     the destination except in the case of extension printers.
     * @private
     */
    onCapabilitiesSet_(origin, id, settingsInfo) {
      let dest = null;
      if (origin !== print_preview.DestinationOrigin.PRIVET) {
        const key = this.getDestinationKey_(origin, id, '');
        dest = this.destinationMap_[key];
      }
      if (!dest) {
        // Ignore unrecognized extension printers
        if (!settingsInfo.printer) {
          assert(origin === print_preview.DestinationOrigin.EXTENSION);
          return;
        }
        dest = print_preview.parseDestination(
            print_preview.originToType(origin), assert(settingsInfo.printer));
      }
      if (dest) {
        if ((origin === print_preview.DestinationOrigin.LOCAL ||
             origin === print_preview.DestinationOrigin.CROS) &&
            dest.capabilities) {
          // If capabilities are already set for this destination ignore new
          // results. This prevents custom margins from being cleared as long
          // as the user does not change to a new non-recent destination.
          return;
        }
        const updateDestination = destination => {
          destination.capabilities = settingsInfo.capabilities;
          this.updateDestination_(destination);
        };
        if (Array.isArray(dest)) {
          dest.forEach(updateDestination);
        } else {
          updateDestination(dest);
        }
      }
    }

    /**
     * Called when a request to get a local destination's print capabilities
     * fails. If the destination is the initial destination, auto-select another
     * destination instead.
     * @param {print_preview.DestinationOrigin} origin The origin type of the
     *     failed destination.
     * @param {string} destinationId The destination ID that failed.
     * @private
     */
    onGetCapabilitiesFail_(origin, destinationId) {
      console.warn(
          'Failed to get print capabilities for printer ' + destinationId);
      if (this.selectedDestination_ &&
          this.selectedDestination_.id == destinationId) {
        const event =
            new Event(DestinationStore.EventType.SELECTED_DESTINATION_INVALID);
        event.destinationId = destinationId;
        this.dispatchEvent(event);
      }
      if (this.autoSelectMatchingDestination_ &&
          this.autoSelectMatchingDestination_.matchIdAndOrigin(
              destinationId, origin)) {
        this.selectDefaultDestination_();
      }
    }

    /**
     * Called when the /search call completes, either successfully or not.
     * In case of success, stores fetched destinations.
     * @param {Event} event Contains the request result.
     * @private
     */
    onCloudPrintSearchDone_(event) {
      if (event.printers) {
        this.insertDestinations_(event.printers);
      }
      if (event.searchDone) {
        const origins = this.loadedCloudOrigins_[event.user] || [];
        if (origins.indexOf(event.origin) < 0) {
          this.loadedCloudOrigins_[event.user] = origins.concat([event.origin]);
        }
      }
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATION_SEARCH_DONE);
    }

    /**
     * Called when /printer call completes. Updates the specified destination's
     * print capabilities.
     * @param {Event} event Contains detailed information about the
     *     destination.
     * @private
     */
    onCloudPrintPrinterDone_(event) {
      this.updateDestination_(event.printer);
    }

    /**
     * Called when the Google Cloud Print interface fails to lookup a
     * destination. Selects another destination if the failed destination was
     * the initial destination.
     * @param {Object} event Contains the ID of the destination that was failed
     *     to be looked up.
     * @private
     */
    onCloudPrintPrinterFailed_(event) {
      if (this.autoSelectMatchingDestination_ &&
          this.autoSelectMatchingDestination_.matchIdAndOrigin(
              event.destinationId, event.destinationOrigin)) {
        console.error(
            'Failed to fetch last used printer caps: ' + event.destinationId);
        this.selectDefaultDestination_();
      }
    }

    /**
     * Called when printer sharing invitation was processed successfully.
     * @param {Event} event Contains detailed information about the invite and
     *     newly accepted destination (if known).
     * @private
     */
    onCloudPrintProcessInviteDone_(event) {
      if (event.accept && event.printer) {
        // Hint the destination list to promote this new destination.
        event.printer.isRecent = true;
        this.insertDestination_(event.printer);
      }
    }

    /**
     * Called when a printer or printers are detected after sending getPrinters
     * from the native layer.
     * @param {print_preview.PrinterType} type The type of printer(s) added.
     * @param {!Array<!print_preview.LocalDestinationInfo |
     *                !print_preview.PrivetPrinterDescription |
     *                !print_preview.ProvisionalDestinationInfo>} printers
     *     Information about the printers that have been retrieved.
     */
    onPrintersAdded_(type, printers) {
      if (type == print_preview.PrinterType.PRIVET_PRINTER) {
        const printer =
            /** !print_preview.PrivetPrinterDescription */ (printers[0]);
        if (printer.serviceName == this.waitForRegisterDestination_ &&
            !printer.isUnregistered) {
          this.waitForRegisterDestination_ = null;
          this.onDestinationsReload();
          return;
        }
      }
      this.insertDestinations_(printers.map(
          printer => print_preview.parseDestination(type, printer)));
    }

    /**
     * Called from print preview after the user was requested to sign in, and
     * did so successfully.
     */
    onDestinationsReload() {
      this.reset_();
      this.autoSelectMatchingDestination_ =
          this.convertPreselectedToDestinationMatch_();
      this.createLocalPdfPrintDestination_();
      this.startLoadAllDestinations();
    }

    // TODO(vitalybuka): Remove three next functions replacing Destination.id
    //    and Destination.origin by complex ID.
    /**
     * Returns key to be used with {@code destinationMap_}.
     * @param {!print_preview.DestinationOrigin} origin Destination origin.
     * @param {string} id Destination id.
     * @param {string} account User account destination is registered for.
     * @private
     */
    getDestinationKey_(origin, id, account) {
      return origin + '/' + id + '/' + account;
    }

    /**
     * Returns key to be used with {@code destinationMap_}.
     * @param {!print_preview.Destination} destination Destination.
     * @private
     */
    getKey_(destination) {
      return this.getDestinationKey_(
          destination.origin, destination.id, destination.account);
    }
  }

  /**
   * Event types dispatched by the data store.
   * @enum {string}
   */
  DestinationStore.EventType = {
    DESTINATION_SEARCH_DONE:
        'print_preview.DestinationStore.DESTINATION_SEARCH_DONE',
    DESTINATION_SEARCH_STARTED:
        'print_preview.DestinationStore.DESTINATION_SEARCH_STARTED',
    DESTINATION_SELECT: 'print_preview.DestinationStore.DESTINATION_SELECT',
    DESTINATIONS_RESET: 'print_preview.DestinationStore.DESTINATIONS_RESET',
    DESTINATIONS_INSERTED:
        'print_preview.DestinationStore.DESTINATIONS_INSERTED',
    PROVISIONAL_DESTINATION_RESOLVED:
        'print_preview.DestinationStore.PROVISIONAL_DESTINATION_RESOLVED',
    CACHED_SELECTED_DESTINATION_INFO_READY:
        'print_preview.DestinationStore.CACHED_SELECTED_DESTINATION_INFO_READY',
    SELECTED_DESTINATION_CAPABILITIES_READY: 'print_preview.DestinationStore' +
        '.SELECTED_DESTINATION_CAPABILITIES_READY',
    SELECTED_DESTINATION_INVALID:
        'print_preview.DestinationStore.SELECTED_DESTINATION_INVALID',
    SELECTED_DESTINATION_UNSUPPORTED:
        'print_preview.DestinationStore.SELECTED_DESTINATION_UNSUPPORTED',
  };

  /**
   * Delay in milliseconds before the destination store ignores the initial
   * destination ID and just selects any printer (since the initial destination
   * was not found).
   * @private {number}
   * @const
   */
  DestinationStore.AUTO_SELECT_TIMEOUT_ = 15000;

  /**
   * Maximum amount of time spent searching for extension destinations, in
   * milliseconds.
   * @private {number}
   * @const
   */
  DestinationStore.EXTENSION_SEARCH_DURATION_ = 5000;

  /**
   * Human readable names for media sizes in the cloud print CDD.
   * https://developers.google.com/cloud-print/docs/cdd
   * @private {Object<string>}
   * @const
   */
  DestinationStore.MEDIA_DISPLAY_NAMES_ = {
    'ISO_2A0': '2A0',
    'ISO_A0': 'A0',
    'ISO_A0X3': 'A0x3',
    'ISO_A1': 'A1',
    'ISO_A10': 'A10',
    'ISO_A1X3': 'A1x3',
    'ISO_A1X4': 'A1x4',
    'ISO_A2': 'A2',
    'ISO_A2X3': 'A2x3',
    'ISO_A2X4': 'A2x4',
    'ISO_A2X5': 'A2x5',
    'ISO_A3': 'A3',
    'ISO_A3X3': 'A3x3',
    'ISO_A3X4': 'A3x4',
    'ISO_A3X5': 'A3x5',
    'ISO_A3X6': 'A3x6',
    'ISO_A3X7': 'A3x7',
    'ISO_A3_EXTRA': 'A3 Extra',
    'ISO_A4': 'A4',
    'ISO_A4X3': 'A4x3',
    'ISO_A4X4': 'A4x4',
    'ISO_A4X5': 'A4x5',
    'ISO_A4X6': 'A4x6',
    'ISO_A4X7': 'A4x7',
    'ISO_A4X8': 'A4x8',
    'ISO_A4X9': 'A4x9',
    'ISO_A4_EXTRA': 'A4 Extra',
    'ISO_A4_TAB': 'A4 Tab',
    'ISO_A5': 'A5',
    'ISO_A5_EXTRA': 'A5 Extra',
    'ISO_A6': 'A6',
    'ISO_A7': 'A7',
    'ISO_A8': 'A8',
    'ISO_A9': 'A9',
    'ISO_B0': 'B0',
    'ISO_B1': 'B1',
    'ISO_B10': 'B10',
    'ISO_B2': 'B2',
    'ISO_B3': 'B3',
    'ISO_B4': 'B4',
    'ISO_B5': 'B5',
    'ISO_B5_EXTRA': 'B5 Extra',
    'ISO_B6': 'B6',
    'ISO_B6C4': 'B6C4',
    'ISO_B7': 'B7',
    'ISO_B8': 'B8',
    'ISO_B9': 'B9',
    'ISO_C0': 'C0',
    'ISO_C1': 'C1',
    'ISO_C10': 'C10',
    'ISO_C2': 'C2',
    'ISO_C3': 'C3',
    'ISO_C4': 'C4',
    'ISO_C5': 'C5',
    'ISO_C6': 'C6',
    'ISO_C6C5': 'C6C5',
    'ISO_C7': 'C7',
    'ISO_C7C6': 'C7C6',
    'ISO_C8': 'C8',
    'ISO_C9': 'C9',
    'ISO_DL': 'Envelope DL',
    'ISO_RA0': 'RA0',
    'ISO_RA1': 'RA1',
    'ISO_RA2': 'RA2',
    'ISO_SRA0': 'SRA0',
    'ISO_SRA1': 'SRA1',
    'ISO_SRA2': 'SRA2',
    'JIS_B0': 'B0 (JIS)',
    'JIS_B1': 'B1 (JIS)',
    'JIS_B10': 'B10 (JIS)',
    'JIS_B2': 'B2 (JIS)',
    'JIS_B3': 'B3 (JIS)',
    'JIS_B4': 'B4 (JIS)',
    'JIS_B5': 'B5 (JIS)',
    'JIS_B6': 'B6 (JIS)',
    'JIS_B7': 'B7 (JIS)',
    'JIS_B8': 'B8 (JIS)',
    'JIS_B9': 'B9 (JIS)',
    'JIS_EXEC': 'Executive (JIS)',
    'JPN_CHOU2': 'Choukei 2',
    'JPN_CHOU3': 'Choukei 3',
    'JPN_CHOU4': 'Choukei 4',
    'JPN_HAGAKI': 'Hagaki',
    'JPN_KAHU': 'Kahu Envelope',
    'JPN_KAKU2': 'Kaku 2',
    'JPN_OUFUKU': 'Oufuku Hagaki',
    'JPN_YOU4': 'You 4',
    'NA_10X11': '10x11',
    'NA_10X13': '10x13',
    'NA_10X14': '10x14',
    'NA_10X15': '10x15',
    'NA_11X12': '11x12',
    'NA_11X15': '11x15',
    'NA_12X19': '12x19',
    'NA_5X7': '5x7',
    'NA_6X9': '6x9',
    'NA_7X9': '7x9',
    'NA_9X11': '9x11',
    'NA_A2': 'A2',
    'NA_ARCH_A': 'Arch A',
    'NA_ARCH_B': 'Arch B',
    'NA_ARCH_C': 'Arch C',
    'NA_ARCH_D': 'Arch D',
    'NA_ARCH_E': 'Arch E',
    'NA_ASME_F': 'ASME F',
    'NA_B_PLUS': 'B-plus',
    'NA_C': 'C',
    'NA_C5': 'C5',
    'NA_D': 'D',
    'NA_E': 'E',
    'NA_EDP': 'EDP',
    'NA_EUR_EDP': 'European EDP',
    'NA_EXECUTIVE': 'Executive',
    'NA_F': 'F',
    'NA_FANFOLD_EUR': 'FanFold European',
    'NA_FANFOLD_US': 'FanFold US',
    'NA_FOOLSCAP': 'FanFold German Legal',
    'NA_GOVT_LEGAL': 'Government Legal',
    'NA_GOVT_LETTER': 'Government Letter',
    'NA_INDEX_3X5': 'Index 3x5',
    'NA_INDEX_4X6': 'Index 4x6',
    'NA_INDEX_4X6_EXT': 'Index 4x6 ext',
    'NA_INDEX_5X8': '5x8',
    'NA_INVOICE': 'Invoice',
    'NA_LEDGER': 'Tabloid',  // Ledger in portrait is called Tabloid.
    'NA_LEGAL': 'Legal',
    'NA_LEGAL_EXTRA': 'Legal extra',
    'NA_LETTER': 'Letter',
    'NA_LETTER_EXTRA': 'Letter extra',
    'NA_LETTER_PLUS': 'Letter plus',
    'NA_MONARCH': 'Monarch',
    'NA_NUMBER_10': 'Envelope #10',
    'NA_NUMBER_11': 'Envelope #11',
    'NA_NUMBER_12': 'Envelope #12',
    'NA_NUMBER_14': 'Envelope #14',
    'NA_NUMBER_9': 'Envelope #9',
    'NA_PERSONAL': 'Personal',
    'NA_QUARTO': 'Quarto',
    'NA_SUPER_A': 'Super A',
    'NA_SUPER_B': 'Super B',
    'NA_WIDE_FORMAT': 'Wide format',
    'OM_DAI_PA_KAI': 'Dai-pa-kai',
    'OM_FOLIO': 'Folio',
    'OM_FOLIO_SP': 'Folio SP',
    'OM_INVITE': 'Invite Envelope',
    'OM_ITALIAN': 'Italian Envelope',
    'OM_JUURO_KU_KAI': 'Juuro-ku-kai',
    'OM_LARGE_PHOTO': 'Large photo',
    'OM_OFICIO': 'Oficio',
    'OM_PA_KAI': 'Pa-kai',
    'OM_POSTFIX': 'Postfix Envelope',
    'OM_SMALL_PHOTO': 'Small photo',
    'PRC_1': 'prc1 Envelope',
    'PRC_10': 'prc10 Envelope',
    'PRC_16K': 'prc 16k',
    'PRC_2': 'prc2 Envelope',
    'PRC_3': 'prc3 Envelope',
    'PRC_32K': 'prc 32k',
    'PRC_4': 'prc4 Envelope',
    'PRC_5': 'prc5 Envelope',
    'PRC_6': 'prc6 Envelope',
    'PRC_7': 'prc7 Envelope',
    'PRC_8': 'prc8 Envelope',
    'ROC_16K': 'ROC 16K',
    'ROC_8K': 'ROC 8k',
  };

  // Export
  return {DestinationStore: DestinationStore};
});

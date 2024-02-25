// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './color_mode_select.js';
import './file_type_select.js';
import './loading_page.js';
import './multi_page_checkbox.js';
import './multi_page_scan.js';
import './page_size_select.js';
import './resolution_select.js';
import './scan_done_section.js';
import './scan_preview.js';
import './scan_to_select.js';
import './scanner_select.js';
import './scanning_fonts.css.js';
import './scanning_shared.css.js';
import './source_select.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrContainerShadowMixin, CrContainerShadowMixinInterface} from 'chrome://resources/ash/common/cr_elements/cr_container_shadow_mixin.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {CrToastElement} from 'chrome://resources/ash/common/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import {IronCollapseElement} from 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getScanService} from './mojo_interface_provider.js';
import {ColorMode, FileType, MultiPageScanControllerRemote, PageSize, ScanJobObserverInterface, ScanJobObserverReceiver, Scanner, ScannerCapabilities, ScanResult, ScanSettings as ScanSettingsMojom, SourceType} from './scanning.mojom-webui.js';
import {getTemplate} from './scanning_app.html.js';
import {AppState, MAX_NUM_SAVED_SCANNERS, ScanJobSettingsForMetrics, ScannerCapabilitiesResponse, ScannerInfo, ScannerSetting, ScannersReceivedResponse, ScanSettings, StartMultiPageScanResponse, SuccessResponse} from './scanning_app_types.js';
import {colorModeFromString, fileTypeFromString, getScannerDisplayName, pageSizeFromString, tokenToString} from './scanning_app_util.js';
import {ScanningBrowserProxyImpl} from './scanning_browser_proxy.js';

/**
 * URL for the Scanning help page.
 */
const HELP_PAGE_LINK = 'http://support.google.com/chromebook?p=chrome_scanning';

// Pages are counted using natural numbering.
const INITIAL_PAGE_NUMBER = 1;
const INITIAL_PROGRESS_PERCENT = 0;

/**
 * @fileoverview
 * 'scanning-app' is used to interact with connected scanners.
 */

const ScanningAppElementBase = CrContainerShadowMixin(
                                   I18nMixin(PolymerElement)) as {
  new (): PolymerElement & I18nMixinInterface & CrContainerShadowMixinInterface,
};

export class ScanningAppElement extends ScanningAppElementBase implements
    ScanJobObserverInterface {
  static get is() {
    return 'scanning-app' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      scanners: {
        type: Array,
        value: () => [],
      },

      selectedScannerId: {
        type: String,
        observer: ScanningAppElement.prototype.selectedScannerIdChanged,
      },

      capabilities: Object,


      selectedSource: String,


      selectedFileType: String,


      selectedFilePath: String,


      selectedColorMode: String,


      selectedPageSize: String,


      selectedResolution: String,

      /**
       * Used to indicate where scanned files are saved when a scan is complete.
       */
      selectedFolder: String,

      /**
       * Map of a ScanSource's name to its corresponding SourceType. Used for
       * fetching the SourceType setting for scan job metrics.
       */
      sourceTypeMap: {
        type: Object,
        value() {
          return new Map();
        },
      },

      /**
       * Used to determine when certain parts of the app should be shown or
       * hidden and enabled or disabled.
       */
      appState: {
        type: Number,
        value: AppState.GETTING_SCANNERS,
        observer: ScanningAppElement.prototype.appStateChanged,
      },

      /**
       * The object URLs of the scanned images.
       */
      objectUrls: {
        type: Array,
        value: () => [],
      },

      /**
       * Used to display which page is being scanned during a scan.
       */
      pageNumber: {
        type: Number,
        value: INITIAL_PAGE_NUMBER,
      },

      /**
       * Used to display a page's scan progress during a scan.
       */
      progressPercent: {
        type: Number,
        value: INITIAL_PROGRESS_PERCENT,
      },

      selectedSourceColorModes: {
        type: Array,
        value: () => [],
        computed: 'computeSelectedSourceColorModes(' +
            'selectedSource, capabilities.sources)',
      },

      selectedSourcePageSizes: {
        type: Array,
        value: () => [],
        computed: 'computeSelectedSourcePageSizes(' +
            'selectedSource, capabilities.sources)',
      },

      selectedSourceResolutions: {
        type: Array,
        value: () => [],
        computed: 'computeSelectedSourceResolutions(' +
            'selectedSource, capabilities.sources)',
      },

      /**
       * Determines whether settings should be disabled based on the current app
       * state. Settings should be disabled until after the selected scanner's
       * capabilities are fetched since the capabilities determine what options
       * are available in the settings. They should also be disabled while
       * scanning since settings cannot be changed while a scan is in progress.
       */
      settingsDisabled: {
        type: Boolean,
        value: true,
      },

      scannersLoaded: {
        type: Boolean,
        value: false,
      },

      showDoneSection: {
        type: Boolean,
        value: false,
      },

      showCancelButton: {
        type: Boolean,
        value: false,
      },

      cancelButtonDisabled: {
        type: Boolean,
        value: false,
      },

      /**
       * The file paths of the scanned pages of a successful scan job.
       */
      scannedFilePaths: {
        type: Array,
        value: () => [],
      },

      /**
       * The key to retrieve the appropriate string to display in the toast.
       */
      toastMessageKey: {
        type: String,
        observer: ScanningAppElement.prototype.toastMessageKeyChanged,
      },

      showToastInfoIcon: {
        type: Boolean,
        value: false,
      },

      showToastHelpLink: {
        type: Boolean,
        value: false,
      },

      /**
       * Indicates whether the More settings section is expanded.
       */
      opened: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /**
       * Determines the arrow icon direction.
       */
      arrowIconDirection: {
        type: String,
        computed: 'computeArrowIconDirection(opened)',
      },

      /**
       * Used to track the number of times a user changes scan settings before
       * initiating a scan. This gets reset to 1 when the user selects a
       * different scanner (selecting a different scanner is treated as a
       * setting change).
       */
      numScanSettingChanges: {
        type: Number,
        value: 0,
      },

      /**
       * The key to retrieve the appropriate string to display in an error
       * dialog when a scan job fails.
       */
      scanFailedDialogTextKey: String,

      savedScanSettings: {
        type: Object,
        value() {
          return {
            lastUsedScannerName: '',
            scanToPath: '',
            scanners: [],
          };
        },
      },

      lastUsedScannerId: String,

      /**
       * Used to track the number of completed scans during a single session of
       * the Scan app being open. This value is recorded whenever the app window
       * is closed or refreshed.
       */
      numCompletedScansInSession: {
        type: Number,
        value: 0,
      },

      multiPageScanChecked: Boolean,

      /**
       * Only true when the multi-page checkbox is checked and the supported
       * scan settings are chosen. Multi-page scanning only supports creating
       * PDFs from the Flatbed source.
       */
      isMultiPageScan: {
        type: Boolean,
        computed: 'computeIsMultiPageScan(multiPageScanChecked, ' +
            'selectedFileType, selectedSource)',
        observer: 'onIsMultiPageScanChange',
      },

      showMultiPageCheckbox: {
        type: Boolean,
        computed: 'computeShowMultiPageCheckbox(showScanSettings, ' +
            'selectedSource, selectedFileType)',
        reflectToAttribute: true,
      },

      scanButtonText: String,

      showScanSettings: {
        type: Boolean,
        value: true,
      },

      showMultiPageScan: {
        type: Boolean,
        value: false,
      },
    };
  }

  static get observers() {
    return [
      'scanSettingsChange(selectedSource, selectedFileType, ' +
          'selectedFilePath, selectedColorMode, selectedPageSize, ' +
          'selectedResolution)',
    ];
  }

  selectedScannerId: string;
  selectedSource: string;
  selectedFileType: string;
  selectedFilePath: string;
  selectedColorMode: string;
  selectedPageSize: string;
  selectedResolution: string;
  selectedFolder: string;
  multiPageScanChecked: boolean;
  private scanners: Scanner[];
  private capabilities: ScannerCapabilities|null;
  private appState: AppState;
  private objectUrls: string[];
  private pageNumber: number;
  private progressPercent: number;
  private selectedSourceColorModes: ColorMode[];
  private selectedSourcePageSizes: PageSize[];
  private selectedSourceResolutions: number[];
  private settingsDisabled: boolean;
  private scannersLoaded: boolean;
  private showDoneSection: boolean;
  private showCancelButton: boolean;
  private cancelButtonDisabled: boolean;
  private scannedFilePaths: FilePath[];
  private toastMessageKey: string;
  private showToastInfoIcon: boolean;
  private showToastHelpLink: boolean;
  private opened: boolean;
  private arrowIconDirection: string;
  private numScanSettingChanges: number;
  private scanFailedDialogTextKey: string;
  private savedScanSettings: ScanSettings;
  private lastUsedScannerId: string;
  private numCompletedScansInSession: number;
  private isMultiPageScan: boolean;
  private showMultiPageCheckbox: boolean;
  private scanButtonText: string;
  private showScanSettings: boolean;
  private showMultiPageScan: boolean;
  private scanJobObserverReceiver: ScanJobObserverReceiver|null;
  private multiPageScanController: MultiPageScanControllerRemote|null;
  private scanService = getScanService();
  private browserProxy = ScanningBrowserProxyImpl.getInstance();
  private sourceTypeMap = new Map<string, SourceType>();
  private scannerInfoMap = new Map<string, ScannerInfo>();

  constructor() {
    super();

    this.browserProxy.initialize();
    this.browserProxy.getMyFilesPath().then((myFilesPath) => {
      this.selectedFilePath = myFilesPath;
    });
    this.browserProxy.getScanSettings().then((scanSettings) => {
      if (!scanSettings) {
        return;
      }

      this.savedScanSettings = (JSON.parse(scanSettings));
    });
  }

  override ready() {
    super.ready();

    window.addEventListener('beforeunload', event => {
      this.browserProxy.recordNumCompletedScans(
          this.numCompletedScansInSession);

      // When the user tries to close the app while a scan is in progress,
      // show the 'Leave site' dialog.
      if (this.appState === AppState.SCANNING) {
        event.preventDefault();
        event.returnValue = '';
      }
    });

    this.scanService.getScanners().then(
        (response: ScannersReceivedResponse) => {
          this.onScannersReceived(response);
        });
  }

  override connectedCallback() {
    super.connectedCallback();

    /** @suppress {checkTypes} */
    (function() {
      ColorChangeUpdater.forDocument().start();
    })();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    if (this.scanJobObserverReceiver) {
      this.scanJobObserverReceiver.$.close();
    }
  }

  /**
   * Overrides ScanJobObserverInterface.
   */
  onPageProgress(pageNumber: number, progressPercent: number): void {
    assert(
        this.appState === AppState.SCANNING ||
        this.appState === AppState.MULTI_PAGE_SCANNING ||
        this.appState === AppState.CANCELING ||
        this.appState === AppState.MULTI_PAGE_CANCELING);

    // The Scan app increments |this.pageNumber| itself during a multi-page
    // scan.
    if (!this.isMultiPageScan) {
      this.pageNumber = pageNumber;
    }
    this.progressPercent = progressPercent;
  }

  /**
   * Overrides ScanJobObserverInterface.
   */
  onPageComplete(pageData: number[], newPageIndex: number): void {
    assert(
        this.appState === AppState.SCANNING ||
        this.appState === AppState.MULTI_PAGE_SCANNING ||
        this.appState === AppState.CANCELING ||
        this.appState === AppState.MULTI_PAGE_CANCELING);

    const blob = new Blob([Uint8Array.from(pageData)], {'type': 'image/png'});
    const objectUrl = URL.createObjectURL(blob);
    if (newPageIndex === this.objectUrls.length) {
      this.push('objectUrls', objectUrl);
    } else {
      this.splice('objectUrls', newPageIndex, 1, objectUrl);
    }

    // |pageNumber| gets set to the number of existing scanned images so
    // when the next scan is started, |pageNumber| gets incremented and
    // the preview area shows 'Scanning length+1'.
    this.pageNumber = this.objectUrls.length;

    if (this.isMultiPageScan) {
      this.setAppState(AppState.MULTI_PAGE_NEXT_ACTION);
    }
  }

  /**
   * Overrides ScanJobObserverInterface.
   */
  onScanComplete(result: ScanResult, scannedFilePaths: FilePath[]): void {
    if (result !== ScanResult.kSuccess || this.objectUrls.length == 0) {
      this.setScanFailedDialogTextKey(result);
      strictQuery('#scanFailedDialog', this.shadowRoot, CrDialogElement)
          .showModal();
      return;
    }

    ++this.numCompletedScansInSession;
    this.scannedFilePaths = scannedFilePaths;
    this.setAppState(AppState.DONE);
  }

  /**
   * Overrides ScanJobObserverInterface.
   */
  onCancelComplete(success: boolean): void {
    // If the cancel request fails, continue showing the scan progress page.
    if (!success) {
      this.showToast('cancelFailedToastText');
      this.setAppState(
          this.appState === AppState.MULTI_PAGE_CANCELING ?
              AppState.MULTI_PAGE_SCANNING :
              AppState.SCANNING);
      return;
    }

    if (this.appState === AppState.MULTI_PAGE_CANCELING) {
      // For multi-page scans |pageNumber| needs to be set to the number of
      // existing scanned images since the next scan isn't guaranteed to be the
      // first page. So when the next scan is started, |pageNumber| will get
      // incremented and the preview area will show 'Scanning length+1'.
      this.pageNumber = this.objectUrls.length;
      this.setAppState(AppState.MULTI_PAGE_NEXT_ACTION);
    } else {
      this.setAppState(AppState.READY);
    }

    this.showToast('scanCanceledToastText');
  }

  /**
   * Overrides ScanJobObserverInterface.
   */
  onMultiPageScanFail(result: ScanResult): void {
    assert(result !== ScanResult.kSuccess);

    this.setScanFailedDialogTextKey(result);
    strictQuery('#scanFailedDialog', this.shadowRoot, CrDialogElement)
        .showModal();
  }

  private computeSelectedSourceColorModes(): ColorMode[] {
    assert(this.capabilities);
    for (const source of this.capabilities!.sources) {
      if (source.name === this.selectedSource) {
        return source.colorModes;
      }
    }

    return [];
  }

  private computeSelectedSourcePageSizes(): PageSize[] {
    assert(this.capabilities);
    for (const source of this.capabilities!.sources) {
      if (source.name === this.selectedSource) {
        return source.pageSizes;
      }
    }

    return [];
  }

  private computeSelectedSourceResolutions(): number[] {
    assert(this.capabilities);
    for (const source of this.capabilities!.sources) {
      if (source.name === this.selectedSource) {
        return source.resolutions;
      }
    }

    return [];
  }

  private onCapabilitiesReceived(capabilities: ScannerCapabilities): void {
    this.capabilities = capabilities;
    this.capabilities.sources.forEach(
        (source) => this.sourceTypeMap.set(source.name, source.type));
    this.selectedFileType = FileType.kPdf.toString();

    this.setAppState(
        this.areSavedScanSettingsAvailable() ? AppState.SETTING_SAVED_SETTINGS :
                                               AppState.READY);
  }

  private onScannersReceived(response: ScannersReceivedResponse): void {
    if (response.scanners.length === 0) {
      this.setAppState(AppState.NO_SCANNERS);
      return;
    }

    for (const scanner of response.scanners) {
      this.setScannerInfo(scanner);
      if (this.isLastUsedScanner(scanner)) {
        this.lastUsedScannerId = tokenToString(scanner.id);
      }
    }

    this.setAppState(AppState.GOT_SCANNERS);
    this.scanners = response.scanners;
  }

  private selectedScannerIdChanged(): void {
    assert(this.isSelectedScannerKnown());

    // If |selectedScannerId| is changed when the app's in a non-READY state,
    // that change was triggered by the app's initial load so it's not counted.
    this.numScanSettingChanges = this.appState === AppState.READY ? 1 : 0;
    this.setAppState(AppState.GETTING_CAPS);

    this.getSelectedScannerCapabilities().then(
        (response: ScannerCapabilitiesResponse) => {
          this.onCapabilitiesReceived(response.capabilities);
        });
  }

  private onScanClick(): void {
    // Force hide the toast if user attempts a new scan before the toast times
    // out.
    strictQuery('#toast', this.shadowRoot, CrToastElement).hide();

    if (!this.selectedScannerId || !this.selectedSource ||
        !this.selectedFileType || !this.selectedColorMode ||
        !this.selectedPageSize || !this.selectedResolution) {
      this.showToast('startScanFailedToast');
      return;
    }

    if (!this.scanJobObserverReceiver) {
      this.scanJobObserverReceiver = new ScanJobObserverReceiver(this);
    }

    const settings = this.getScanSettings();
    if (this.isMultiPageScan) {
      this.scanService
          .startMultiPageScan(
              this.getSelectedScannerToken(), settings,
              this.scanJobObserverReceiver.$.bindNewPipeAndPassRemote())
          .then(

              (response: StartMultiPageScanResponse) => {
                this.onStartMultiPageScanResponse(response);
              });
    } else {
      this.scanService
          .startScan(
              this.getSelectedScannerToken(), settings,
              this.scanJobObserverReceiver.$.bindNewPipeAndPassRemote())
          .then((response: SuccessResponse) => {
            this.onStartScanResponse(response);
          });
    }

    this.saveScanSettings();

    const scanJobSettingsForMetrics = {
      sourceType: this.sourceTypeMap.get(this.selectedSource),
      fileType: settings.fileType,
      colorMode: settings.colorMode,
      pageSize: settings.pageSize,
      resolution: settings.resolutionDpi,
    } as ScanJobSettingsForMetrics;
    this.browserProxy.recordScanJobSettings(scanJobSettingsForMetrics);

    this.browserProxy.recordNumScanSettingChanges(this.numScanSettingChanges);
    this.numScanSettingChanges = 0;
  }

  private onDoneClick(): void {
    this.setAppState(AppState.READY);
  }

  private onStartScanResponse(response: SuccessResponse): void {
    if (!response.success) {
      this.showToast('startScanFailedToast');
      return;
    }

    this.setAppState(AppState.SCANNING);
    this.pageNumber = INITIAL_PAGE_NUMBER;
    this.progressPercent = INITIAL_PROGRESS_PERCENT;
  }

  private onStartMultiPageScanResponse(response: StartMultiPageScanResponse):
      void {
    if (!response.controller) {
      this.showToast('startScanFailedToast');
      return;
    }

    this.setAppState(AppState.SCANNING);

    assert(!this.multiPageScanController);
    this.multiPageScanController = response.controller;
    this.pageNumber = INITIAL_PAGE_NUMBER;
    this.progressPercent = INITIAL_PROGRESS_PERCENT;
  }

  private onScanNextPage(): void {
    this.multiPageScanController!
        .scanNextPage(this.getSelectedScannerToken(), this.getScanSettings())
        .then((response: SuccessResponse) => {
          this.onScanNextPageResponse(response);
        });
  }

  onRemovePage(e: CustomEvent<number>): void {
    const pageIndex = e.detail;
    assert(pageIndex >= 0 && pageIndex < this.objectUrls.length);

    this.splice('objectUrls', pageIndex, 1);
    this.pageNumber = this.objectUrls.length;
    this.multiPageScanController!.removePage(pageIndex);

    // If the last page was removed, end the multi-page session and return to
    // the scan settings page.
    if (this.objectUrls.length === 0) {
      this.resetMultiPageScanController();
      this.setAppState(AppState.READY);
    }
  }

  /**
   * Sends the request to initiate a new scan and once completed, use it to
   * replace the existing scanned image at |pageIndex|.
   */
  private onRescanPage(e: CustomEvent<number>): void {
    const pageIndex = e.detail;
    assert(pageIndex >= 0 && pageIndex < this.objectUrls.length);

    this.multiPageScanController!
        .rescanPage(
            this.getSelectedScannerToken(), this.getScanSettings(), pageIndex)
        .then((response: SuccessResponse) => {
          this.onRescanPageResponse(response, pageIndex);
        });
  }

  private onCompleteMultiPageScan(): void {
    assert(this.multiPageScanController);
    this.multiPageScanController!.completeMultiPageScan();
    this.resetMultiPageScanController();
  }

  private resetMultiPageScanController(): void {
    this.multiPageScanController!.$.close();
    this.multiPageScanController = null;
  }

  private onScanNextPageResponse(response: SuccessResponse): void {
    if (!response.success) {
      this.showToast('startScanFailedToast');
      return;
    }

    this.setAppState(AppState.MULTI_PAGE_SCANNING);
    ++this.pageNumber;
    this.progressPercent = INITIAL_PROGRESS_PERCENT;
  }

  private onRescanPageResponse(response: SuccessResponse, pageIndex: number):
      void {
    if (!response.success) {
      this.showToast('startScanFailedToast');
      return;
    }

    this.progressPercent = INITIAL_PROGRESS_PERCENT;
    this.pageNumber = ++pageIndex;
    this.setAppState(AppState.MULTI_PAGE_SCANNING);
  }

  private toggleClicked(): void {
    (strictQuery('#collapse', this.shadowRoot, HTMLElement) as
     IronCollapseElement)
        .toggle();
  }

  private computeArrowIconDirection(): string {
    return this.opened ? 'cr:expand-less' : 'cr:expand-more';
  }

  private getFileSavedText(): string {
    const fileSavedText =
        this.pageNumber > 1 ? 'fileSavedTextPlural' : 'fileSavedText';
    return this.i18n(fileSavedText);
  }

  private onCancelClick(): void {
    assert(
        this.appState === AppState.SCANNING ||
        this.appState === AppState.MULTI_PAGE_SCANNING);
    this.setAppState(
        this.appState === AppState.MULTI_PAGE_SCANNING ?
            AppState.MULTI_PAGE_CANCELING :
            AppState.CANCELING);
    this.scanService.cancelScan();
  }

  /**
   * Revokes and removes all of the object URLs.
   */
  private clearObjectUrls(): void {
    for (const url of this.objectUrls) {
      URL.revokeObjectURL(url);
    }
    this.objectUrls = [];
  }

  /**
   * Sets the app state if the state transition is allowed.
   */
  private setAppState(newState: AppState): void {
    switch (newState) {
      case (AppState.GETTING_SCANNERS):
        assert(
            this.appState === AppState.GETTING_SCANNERS ||
            this.appState === AppState.NO_SCANNERS);
        break;
      case (AppState.GOT_SCANNERS):
        assert(this.appState === AppState.GETTING_SCANNERS);
        break;
      case (AppState.GETTING_CAPS):
        assert(
            this.appState === AppState.GOT_SCANNERS ||
            this.appState === AppState.READY);
        break;
      case (AppState.SETTING_SAVED_SETTINGS):
        assert(this.appState === AppState.GETTING_CAPS);
        break;
      case (AppState.READY):
        assert(
            this.appState === AppState.GETTING_CAPS ||
            this.appState === AppState.SETTING_SAVED_SETTINGS ||
            this.appState === AppState.SCANNING ||
            this.appState === AppState.DONE ||
            this.appState === AppState.CANCELING ||
            this.appState === AppState.MULTI_PAGE_NEXT_ACTION);
        this.clearObjectUrls();
        break;
      case (AppState.SCANNING):
        assert(
            this.appState === AppState.READY ||
            this.appState === AppState.CANCELING);
        break;
      case (AppState.DONE):
        assert(
            this.appState === AppState.SCANNING ||
            this.appState === AppState.CANCELING ||
            this.appState === AppState.MULTI_PAGE_NEXT_ACTION);
        break;
      case (AppState.CANCELING):
        assert(this.appState === AppState.SCANNING);
        break;
      case (AppState.NO_SCANNERS):
        assert(this.appState === AppState.GETTING_SCANNERS);
        break;
      case (AppState.MULTI_PAGE_SCANNING):
        assert(
            this.appState === AppState.MULTI_PAGE_NEXT_ACTION ||
            this.appState === AppState.MULTI_PAGE_CANCELING);
        break;
      case (AppState.MULTI_PAGE_NEXT_ACTION):
        assert(
            this.appState === AppState.SCANNING ||
            this.appState === AppState.CANCELING ||
            this.appState === AppState.MULTI_PAGE_SCANNING ||
            this.appState === AppState.MULTI_PAGE_CANCELING);
        break;
      case (AppState.MULTI_PAGE_CANCELING):
        assert(this.appState === AppState.MULTI_PAGE_SCANNING);
        break;
    }

    this.appState = newState;
  }

  private appStateChanged(): void {
    this.scannersLoaded = this.appState !== AppState.GETTING_SCANNERS &&
        this.appState !== AppState.NO_SCANNERS;
    this.settingsDisabled = this.appState !== AppState.READY;
    this.showCancelButton = this.appState === AppState.SCANNING ||
        this.appState === AppState.CANCELING;
    this.cancelButtonDisabled = this.appState === AppState.CANCELING;
    this.showDoneSection = this.appState === AppState.DONE;
    this.showMultiPageScan =
        this.appState === AppState.MULTI_PAGE_NEXT_ACTION ||
        this.appState === AppState.MULTI_PAGE_SCANNING ||
        this.appState === AppState.MULTI_PAGE_CANCELING;
    this.showScanSettings = !this.showDoneSection && !this.showMultiPageScan;

    // Need to wait for elements to render after updating their disabled and
    // hidden attributes before they can be focused.
    afterNextRender(this, () => {
      if (this.appState === AppState.SETTING_SAVED_SETTINGS) {
        this.setScanSettingsFromSavedSettings();
        this.setAppState(AppState.READY);
      } else if (this.appState === AppState.READY) {
        this.shadowRoot!.querySelector<PolymerElement>('#scannerSelect')!
            .shadowRoot!.querySelector<HTMLElement>('#scannerSelect')!.focus();
      } else if (this.appState === AppState.SCANNING) {
        strictQuery('#cancelButton', this.shadowRoot, CrButtonElement).focus();
      } else if (this.appState === AppState.DONE) {
        this.shadowRoot!.querySelector<PolymerElement>('#scanPreview')!
            .shadowRoot!.querySelector<HTMLElement>('#previewDiv')!.focus();
      }
    });
  }

  private showToast(toastMessageKey: string): void {
    this.toastMessageKey = toastMessageKey;
    strictQuery('#toast', this.shadowRoot, CrToastElement).show();
  }

  private toastMessageKeyChanged(): void {
    this.showToastInfoIcon = this.toastMessageKey !== 'scanCanceledToastText';
    this.showToastHelpLink = this.toastMessageKey !== 'scanCanceledToastText' &&
        this.toastMessageKey !== 'fileNotFoundToastText';
  }

  private onFileNotFound(): void {
    this.showToast('fileNotFoundToastText');
  }

  private onScanFailedDialogOkClick(): void {
    strictQuery('#scanFailedDialog', this.shadowRoot, CrDialogElement).close();
    if (this.appState === AppState.MULTI_PAGE_SCANNING) {
      // |pageNumber| gets set to the number of existing scanned images so
      // when the next scan is started, |pageNumber| gets incremented and
      // the preview area shows 'Scanning length+1'.
      this.pageNumber = this.objectUrls.length;
      this.setAppState(AppState.MULTI_PAGE_NEXT_ACTION);
      return;
    }

    this.setAppState(AppState.READY);
  }

  private onScanFailedDialogGetHelpClick(): void {
    strictQuery('#scanFailedDialog', this.shadowRoot, CrDialogElement).close();
    this.setAppState(AppState.READY);
    window.open(HELP_PAGE_LINK);
  }

  private getNumFilesSaved(): number {
    return this.selectedFileType === FileType.kPdf.toString() ? 1 :
                                                                this.pageNumber;
  }

  private onRetryClick(): void {
    this.setAppState(AppState.GETTING_SCANNERS);
    this.scanService.getScanners().then(
        (response: ScannersReceivedResponse) => {
          this.onScannersReceived(response);
        });
  }

  private onLearnMoreClick(): void {
    window.open(HELP_PAGE_LINK);
  }

  /**
   * Increments the counter for the number of scan setting changes before a
   * scan is initiated.
   */
  private scanSettingsChange(): void {
    // The user can only change scan settings when the app is in READY state. If
    // a setting is changed while the app's in a non-READY state, that change
    // was triggered by the scanner's capabilities loading so it's not counted.
    if (this.appState !== AppState.READY) {
      return;
    }

    ++this.numScanSettingChanges;
  }

  /**
   * ScanResult! indicates the result of the scan job.
   */
  private setScanFailedDialogTextKey(scanResult: ScanResult): void {
    switch (scanResult) {
      case ScanResult.kDeviceBusy:
        this.scanFailedDialogTextKey = 'scanFailedDialogDeviceBusyText';
        break;
      case ScanResult.kAdfJammed:
        this.scanFailedDialogTextKey = 'scanFailedDialogAdfJammedText';
        break;
      case ScanResult.kAdfEmpty:
        this.scanFailedDialogTextKey = 'scanFailedDialogAdfEmptyText';
        break;
      case ScanResult.kFlatbedOpen:
        this.scanFailedDialogTextKey = 'scanFailedDialogFlatbedOpenText';
        break;
      case ScanResult.kIoError:
        this.scanFailedDialogTextKey = 'scanFailedDialogIoErrorText';
        break;
      default:
        this.scanFailedDialogTextKey = 'scanFailedDialogUnknownErrorText';
    }
  }

  private setScanSettingsFromSavedSettings(): void {
    if (!this.areSavedScanSettingsAvailable()) {
      return;
    }

    this.setScanToPathFromSavedSettings();

    const scannerSettings = this.getSelectedScannerSavedSettings();
    if (!scannerSettings) {
      return;
    }

    this.setSelectedSourceTypeIfAvailable(scannerSettings.sourceName);
    afterNextRender(this, () => {
      this.setSelectedFileTypeIfAvailable(scannerSettings.fileType);
      this.setSelectedColorModeIfAvailable(scannerSettings.colorMode);
      this.setSelectedPageSizeIfAvailable(scannerSettings.pageSize);
      this.setSelectedResolutionIfAvailable(scannerSettings.resolutionDpi);
    });

    // This must be set last because it depends on the values of sourceType and
    // fileType.
    this.setMultiPageScanIfAvailable(scannerSettings.multiPageScanChecked);
  }

  private createScannerInfo(scanner: Scanner): ScannerInfo {
    return {
      token: scanner.id,
      displayName: getScannerDisplayName(scanner),
    };
  }

  private setScannerInfo(scanner: Scanner): void {
    this.scannerInfoMap.set(
        tokenToString(scanner.id), this.createScannerInfo(scanner));
  }

  private isLastUsedScanner(scanner: Scanner): boolean {
    return this.savedScanSettings.lastUsedScannerName ===
        getScannerDisplayName(scanner);
  }

  private isSelectedScannerKnown(): boolean {
    return this.scannerInfoMap.has(this.selectedScannerId);
  }

  private getSelectedScannerToken(): UnguessableToken {
    return this.scannerInfoMap.get(this.selectedScannerId)!.token;
  }

  private getSelectedScannerDisplayName(): string {
    return this.scannerInfoMap.get(this.selectedScannerId)!.displayName;
  }

  private getSelectedScannerCapabilities():
      Promise<ScannerCapabilitiesResponse> {
    return this.scanService.getScannerCapabilities(
        this.getSelectedScannerToken());
  }

  private getSelectedScannerSavedSettings(): ScannerSetting|undefined {
    const selectedScannerDisplayName = this.getSelectedScannerDisplayName();
    return this.savedScanSettings.scanners.find(
        (scanner) => scanner.name === selectedScannerDisplayName);
  }

  /**
   * Validates that the file path from saved settings still exists on the local
   * filesystem then sets the proper display name for the 'Scan to' dropdown. If
   * the file path no longer exists, leave the default 'Scan to' path.
   */
  private setScanToPathFromSavedSettings(): void {
    this.browserProxy.ensureValidFilePath(this.savedScanSettings.scanToPath)
        .then((selectedPath) => {
          const baseName = selectedPath.baseName;
          const filePath = selectedPath.filePath;
          if (!baseName || !filePath) {
            return;
          }

          this.selectedFolder = baseName;
          this.selectedFilePath = filePath;
        });
  }

  private saveScanSettings(): void {
    const scannerName = this.getSelectedScannerDisplayName();
    this.savedScanSettings.lastUsedScannerName = scannerName;
    this.savedScanSettings.scanToPath = this.selectedFilePath;

    // Search the scan settings array for the currently selected scanner. If
    // found, replace it with the new scan settings. If not, add it to the list.
    const newScannerSetting = this.createScannerSettingForSelectedScanner();
    const scannerIndex = this.savedScanSettings.scanners.findIndex(
        scanner => scanner.name === scannerName);
    if (scannerIndex === -1) {
      this.savedScanSettings.scanners.push(newScannerSetting);
    } else {
      this.savedScanSettings.scanners[scannerIndex] = newScannerSetting;
    }

    if (this.savedScanSettings.scanners.length > MAX_NUM_SAVED_SCANNERS) {
      this.evictScannersFromScanSettings();
    }

    this.browserProxy.saveScanSettings(JSON.stringify(this.savedScanSettings));
  }

  /**
   * Sort the saved settings scanners array so the oldest scanners are moved to
   * the back then dropped.
   */
  private evictScannersFromScanSettings(): void {
    this.savedScanSettings.scanners.sort(
        (firstScanner, secondScanner): number => {
          const secondScannerDate = new Date(secondScanner.lastScanDate);
          const firstScannerDate = new Date(firstScanner.lastScanDate);
          // Typescript does not allow subtracting Date class. Instead, use
          // `Date.valueOf` to get milliseconds since epoch to calculate sort.
          return secondScannerDate.valueOf() - firstScannerDate.valueOf();
        });
    this.savedScanSettings.scanners.splice(MAX_NUM_SAVED_SCANNERS);
  }

  private createScannerSettingForSelectedScanner(): ScannerSetting {
    return ({
      name: this.getSelectedScannerDisplayName(),
      lastScanDate: new Date(),
      sourceName: this.selectedSource,
      fileType: fileTypeFromString(this.selectedFileType),
      colorMode: colorModeFromString(this.selectedColorMode),
      pageSize: pageSizeFromString(this.selectedPageSize),
      resolutionDpi: Number(this.selectedResolution),
      multiPageScanChecked: this.multiPageScanChecked,
    });
  }

  private areSavedScanSettingsAvailable(): boolean {
    return this.savedScanSettings.scanners.length !== 0;
  }

  private computeShowMultiPageCheckbox(): boolean {
    return this.showScanSettings && this.isPdfSelected() &&
        this.isFlatbedSelected();
  }

  private isPdfSelected(): boolean {
    return !!this.selectedFileType &&
        fileTypeFromString(this.selectedFileType) === FileType.kPdf;
  }

  private isFlatbedSelected(): boolean {
    return !!this.selectedSource &&
        this.sourceTypeMap.get(this.selectedSource) === SourceType.kFlatbed;
  }

  private computeIsMultiPageScan() {
    return this.multiPageScanChecked && this.isPdfSelected() &&
        this.isFlatbedSelected();
  }

  private onIsMultiPageScanChange(): void {
    const nextPageNum = this.isMultiPageScan ? 1 : 0;
    this.browserProxy.getPluralString('scanButtonText', nextPageNum)
        .then((pluralString) => {
          this.scanButtonText = pluralString;
        });
  }

  private getScanSettings(): ScanSettingsMojom {
    const fileType = fileTypeFromString(this.selectedFileType);
    const colorMode = colorModeFromString(this.selectedColorMode);
    const pageSize = pageSizeFromString(this.selectedPageSize);
    const resolution = Number(this.selectedResolution);
    return {
      sourceName: this.selectedSource,
      scanToPath: {path: this.selectedFilePath},
      fileType: fileType,
      colorMode: colorMode,
      pageSize: pageSize,
      resolutionDpi: resolution,
    };
  }

  private setSelectedSourceTypeIfAvailable(sourceName: string): void {
    assert(this.capabilities);
    if (this.capabilities!.sources.find(source => source.name === sourceName)) {
      this.selectedSource = sourceName;
    }
  }

  private setSelectedFileTypeIfAvailable(fileType: FileType): void {
    if (Object.values(FileType).includes(fileType)) {
      this.selectedFileType = fileType.toString();
    }
  }

  private setSelectedColorModeIfAvailable(colorMode: ColorMode): void {
    if (this.selectedSourceColorModes.includes(colorMode)) {
      this.selectedColorMode = colorMode.toString();
    }
  }

  setSelectedPageSizeIfAvailable(pageSize: PageSize) {
    if (this.selectedSourcePageSizes.includes(pageSize)) {
      this.selectedPageSize = pageSize.toString();
    }
  }

  private setSelectedResolutionIfAvailable(resolution: number): void {
    if (this.selectedSourceResolutions.includes(resolution)) {
      this.selectedResolution = resolution.toString();
    }
  }

  private setMultiPageScanIfAvailable(multiPageScanChecked: boolean): void {
    // Only set the checkbox if it's visible (flag is enabled and correct scan
    // settings are selected).
    if (this.showMultiPageCheckbox) {
      this.multiPageScanChecked = multiPageScanChecked;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ScanningAppElement.is]: ScanningAppElement;
  }
}

customElements.define(ScanningAppElement.is, ScanningAppElement);

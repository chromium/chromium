// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import './print_job_clear_history_dialog.js';
import './print_job_entry.js';
import './print_management_fonts.css.js';
import './print_management_shared.css.js';
import './strings.m.js';

import {IronIconElement} from '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getMetadataProvider} from './mojo_interface_provider.js';
import {getTemplate} from './print_management.html.js';
import {ActivePrintJobState, PrintingMetadataProviderInterface, PrintJobInfo, PrintJobsObserverInterface, PrintJobsObserverReceiver} from './printing_manager.mojom-webui.js';

const METADATA_STORED_INDEFINITELY = -1;
const METADATA_STORED_FOR_ONE_DAY = 1;
const METADATA_NOT_STORED = 0;

type RemovePrintJobEvent = CustomEvent<string>;

declare global {
  interface HTMLElementEventMap {
    'remove-print-job': RemovePrintJobEvent;
    'all-history-cleared': CustomEvent<void>;
  }
}

function comparePrintJobsReverseChronologically(
    first: PrintJobInfo, second: PrintJobInfo): number {
  return -comparePrintJobsChronologically(first, second);
}

function comparePrintJobsChronologically(
    first: PrintJobInfo, second: PrintJobInfo): number {
  return Number(first.creationTime.internalValue) -
      Number(second.creationTime.internalValue);
}

/**
 * @fileoverview
 * 'print-management' is used as the main app to display print jobs.
 */

const PrintManagementElementBase = I18nMixin(PolymerElement);

export interface PrintManagementElement {
  $: {deleteIcon: IronIconElement};
}

export class PrintManagementElement extends PrintManagementElementBase
    implements PrintJobsObserverInterface {
  static get is() {
    return 'print-management';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      printJobs_: {
        type: Array,
        value: () => [],
      },

      printJobHistoryExpirationPeriod_: {
        type: String,
        value: '',
      },

      activeHistoryInfoIcon_: {
        type: String,
        value: '',
      },

      isPolicyControlled_: {
        type: Boolean,
        value: false,
      },

      ongoingPrintJobs_: {
        type: Array,
        value: () => [],
      },

      // Used by FocusRowBehavior to track the last focused element on a row.
      lastFocused_: Object,

      // Used by FocusRowBehavior to track if the list has been blurred.
      listBlurred_: Boolean,

      showClearAllButton_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      showClearAllDialog_: {
        type: Boolean,
        value: false,
      },

      deletePrintJobHistoryAllowedByPolicy_: {
        type: Boolean,
        value: true,
      },

      shouldDisableClearAllButton_: {
        type: Boolean,
        computed: 'computeShouldDisableClearAllButton_(printJobs_,' +
            'deletePrintJobHistoryAllowedByPolicy_)',
      },

      /**
       * Receiver responsible for observing print job updates notification
       * events.
       */
      printJobsObserverReceiver_: {type: Object},
    };
  }

  static get observers() {
    return ['onClearAllButtonUpdated_(shouldDisableClearAllButton_)'];
  }

  constructor() {
    super();

    this.mojoInterfaceProvider_ = getMetadataProvider();

    window.CrPolicyStrings = {
      controlledSettingPolicy:
          loadTimeData.getString('clearAllPrintJobPolicyIndicatorToolTip'),
    };

    this.addEventListener('all-history-cleared', () => this.getPrintJobs_());
    this.addEventListener('remove-print-job', (e) => this.removePrintJob_(e));
  }

  private mojoInterfaceProvider_: PrintingMetadataProviderInterface;
  private isPolicyControlled_: boolean;
  private printJobs_: PrintJobInfo[];
  private printJobHistoryExpirationPeriod_: string;
  private activeHistoryInfoIcon_: string;
  private ongoingPrintJobs_: PrintJobInfo[];
  private lastFocused_: Element;
  private listBlurred_: boolean;
  private showClearAllButton_: boolean;
  private showClearAllDialog_: boolean;
  private deletePrintJobHistoryAllowedByPolicy_: boolean;
  private shouldDisableClearAllButton_: boolean;
  private printJobsObserverReceiver_: PrintJobsObserverReceiver;

  override connectedCallback() {
    super.connectedCallback();

    this.getPrintJobHistoryExpirationPeriod_();
    this.startObservingPrintJobs_();
    this.fetchDeletePrintJobHistoryPolicy_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.printJobsObserverReceiver_.$.close();
  }

  private startObservingPrintJobs_() {
    this.printJobsObserverReceiver_ = new PrintJobsObserverReceiver((this));
    this.mojoInterfaceProvider_
        .observePrintJobs(
            this.printJobsObserverReceiver_.$.bindNewPipeAndPassRemote())
        .then(() => {
          this.getPrintJobs_();
        });
  }

  private fetchDeletePrintJobHistoryPolicy_() {
    this.mojoInterfaceProvider_.getDeletePrintJobHistoryAllowedByPolicy().then(
        (param) => {
          this.onGetDeletePrintHistoryPolicy_(param);
        });
  }

  private onGetDeletePrintHistoryPolicy_(responseParam:
                                             {isAllowedByPolicy: boolean}) {
    this.showClearAllButton_ = true;
    this.deletePrintJobHistoryAllowedByPolicy_ =
        responseParam.isAllowedByPolicy;
  }

  onAllPrintJobsDeleted() {
    this.getPrintJobs_();
  }

  onPrintJobUpdate(job: PrintJobInfo) {
    // Only update ongoing print jobs.
    assert(job.activePrintJobInfo);

    // Check if |job| is an existing ongoing print job and requires an update
    // or if |job| is a new ongoing print job.
    const idx = this.getIndexOfOngoingPrintJob_(job.id);
    if (idx !== -1) {
      // Replace the existing ongoing print job with its updated entry.
      this.splice('ongoingPrintJobs_', idx, 1, job);
    } else {
      // New ongoing print jobs are appended to the ongoing print
      // jobs list.
      this.push('ongoingPrintJobs_', job);
    }

    if (job.activePrintJobInfo?.activeState ===
        ActivePrintJobState.kDocumentDone) {
      // This print job is now completed, next step is to update the history
      // list with the recently stored print job.
      this.getPrintJobs_();
    }
  }

  private onPrintJobsReceived_(jobs: {printJobs: PrintJobInfo[]}) {
    // TODO(crbug/1073690): Update this when BigInt is supported for
    // updateList().
    const ongoingList = [];
    const historyList = [];
    for (const job of jobs.printJobs) {
      // activePrintJobInfo is not null for ongoing print jobs.
      if (job.activePrintJobInfo) {
        ongoingList.push(job);
      } else {
        historyList.push(job);
      }
    }

    // Sort the print jobs in chronological order.
    this.ongoingPrintJobs_ = ongoingList.sort(comparePrintJobsChronologically);
    this.printJobs_ = historyList.sort(comparePrintJobsReverseChronologically);
  }

  private getPrintJobs_() {
    this.mojoInterfaceProvider_.getPrintJobs().then(
        this.onPrintJobsReceived_.bind(this));
  }

  private onPrintJobHistoryExpirationPeriodReceived_(printJobPolicyInfo: {
    expirationPeriodInDays: number,
    isFromPolicy: boolean,
  }) {
    const expirationPeriod = printJobPolicyInfo.expirationPeriodInDays;
    // If print jobs are not persisted, we can return early since the tooltip
    // section won't be shown.
    if (expirationPeriod === METADATA_NOT_STORED) {
      return;
    }

    this.isPolicyControlled_ = printJobPolicyInfo.isFromPolicy;
    this.activeHistoryInfoIcon_ =
        this.isPolicyControlled_ ? 'enterpriseIcon' : 'infoIcon';

    switch (expirationPeriod) {
      case METADATA_STORED_INDEFINITELY:
        this.printJobHistoryExpirationPeriod_ =
            loadTimeData.getString('printJobHistoryIndefinitePeriod');
        break;
      case METADATA_STORED_FOR_ONE_DAY:
        this.printJobHistoryExpirationPeriod_ =
            loadTimeData.getString('printJobHistorySingleDay');
        break;
      default:
        this.printJobHistoryExpirationPeriod_ = loadTimeData.getStringF(
            'printJobHistoryExpirationPeriod',
            expirationPeriod,
        );
    }
  }

  private getPrintJobHistoryExpirationPeriod_() {
    this.mojoInterfaceProvider_.getPrintJobHistoryExpirationPeriod().then(
        this.onPrintJobHistoryExpirationPeriodReceived_.bind(this));
  }

  private removePrintJob_(e: RemovePrintJobEvent) {
    const idx = this.getIndexOfOngoingPrintJob_(e.detail);
    if (idx !== -1) {
      this.splice('ongoingPrintJobs_', idx, 1);
    }
  }

  private onClearHistoryClicked_() {
    this.showClearAllDialog_ = true;
  }

  private onClearHistoryDialogClosed_() {
    this.showClearAllDialog_ = false;
  }

  private getIndexOfOngoingPrintJob_(expectedId: string): number {
    return this.ongoingPrintJobs_.findIndex(
        arrJob => arrJob.id === expectedId,
    );
  }

  private computeShouldDisableClearAllButton_(): boolean {
    return !this.deletePrintJobHistoryAllowedByPolicy_ ||
        !this.printJobs_.length;
  }

  private onClearAllButtonUpdated_() {
    this.$.deleteIcon.classList.toggle(
        'delete-enabled', !this.shouldDisableClearAllButton_);
    this.$.deleteIcon.classList.toggle(
        'delete-disabled', this.shouldDisableClearAllButton_);
  }
}

customElements.define(PrintManagementElement.is, PrintManagementElement);

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.DriveSyncHandler = class extends EventTarget {
  /**
   * Returns the completed event name.
   * @return {string}
   */
  getCompletedEventName() {}

  /**
   * Returns whether the Drive sync is currently suppressed or not.
   * @return {boolean}
   */
  isSyncSuppressed() {}

  /**
   * Shows a notification that Drive sync is disabled on cellular networks.
   */
  showDisabledMobileSyncNotification() {}

  /**
   * @return {boolean} Whether the handler is syncing items or not.
   */
  get syncing() {}

  /**
   * Adds a dialog to be controlled by DriveSyncHandler.
   * @param {string} appId App ID of window containing the dialog.
   * @param {DriveDialogControllerInterface} dialog Dialog to be controlled.
   */
  addDialog(appId, dialog) {}
};

window.Crostini = class {
  /**
   * Initialize enabled settings.
   * Must be done after loadTimeData is available.
   */
  initEnabled() {}

  /**
   * Initialize Volume Manager.
   * @param {!VolumeManager} volumeManager
   */
  initVolumeManager(volumeManager) {}

  /**
   * Register for any shared path changes.
   */
  listen() {}

  /**
   * Set whether the specified VM is enabled.
   * @param {string} vmName
   * @param {boolean} enabled
   */
  setEnabled(vmName, enabled) {}

  /**
   * Returns true if the specified VM is enabled.
   * @param {string} vmName
   * @return {boolean}
   */
  isEnabled(vmName) {}

  /**
   * Registers an entry as a shared path for the specified VM.
   * @param {string} vmName
   * @param {!Entry} entry
   */
  registerSharedPath(vmName, entry) {}

  /**
   * Unregisters entry as a shared path from the specified VM.
   * @param {string} vmName
   * @param {!Entry} entry
   */
  unregisterSharedPath(vmName, entry) {}

  /**
   * Returns true if entry is shared with the specified VM.
   * @param {string} vmName
   * @param {!Entry} entry
   * @return {boolean} True if path is shared either by a direct
   *   share or from one of its ancestor directories.
   */
  isPathShared(vmName, entry) {}

  /**
   * Returns true if entry can be shared with the specified VM.
   * @param {string} vmName
   * @param {!Entry} entry
   * @param {boolean} persist If path is to be persisted.
   */
  canSharePath(vmName, entry, persist) {}
};

window.ProgressCenter = class {
  /**
   * Turns off sending updates when a file operation reaches 'completed' state.
   * Used for testing UI that can be ephemeral otherwise.
   */
  neverNotifyCompleted() {}
  /**
   * Updates the item in the progress center.
   * If the item has a new ID, the item is added to the item list.
   * @param {ProgressCenterItem} item Updated item.
   */
  updateItem(item) {}

  /**
   * Requests to cancel the progress item.
   * @param {string} id Progress ID to be requested to cancel.
   */
  requestCancel(id) {}

  /**
   * Adds a panel UI to the notification center.
   * @param {ProgressCenterPanelInterface} panel Panel UI.
   */
  addPanel(panel) {}

  /**
   * Removes a panel UI from the notification center.
   * @param {ProgressCenterPanelInterface} panel Panel UI.
   */
  removePanel(panel) {}

  /**
   * Obtains item by ID.
   * @param {string} id ID of progress item.
   * @return {?ProgressCenterItem} Progress center item having the specified
   *     ID. Null if the item is not found.
   */
  getItemById(id) {}
};

window.FileOperationManager = class extends EventTarget {
  /**
   * Says if there are any tasks in the queue.
   * @return {boolean} True, if there are any tasks.
   */
  hasQueuedTasks() {}

  /**
   * Requests the specified task to be canceled.
   * @param {string} taskId ID of task to be canceled.
   */
  requestTaskCancel(taskId) {}

  /**
   * Filters the entry in the same directory
   *
   * @param {Array<Entry>} sourceEntries Entries of the source files.
   * @param {DirectoryEntry|FakeEntry} targetEntry The destination entry of the
   *     target directory.
   * @param {boolean} isMove True if the operation is "move", otherwise (i.e.
   *     if the operation is "copy") false.
   * @return {Promise} Promise fulfilled with the filtered entry. This is not
   *     rejected.
   */
  filterSameDirectoryEntry(sourceEntries, targetEntry, isMove) {}

  /**
   * Kick off pasting.
   *
   * @param {Array<Entry>} sourceEntries Entries of the source files.
   * @param {DirectoryEntry} targetEntry The destination entry of the target
   *     directory.
   * @param {boolean} isMove True if the operation is "move", otherwise (i.e.
   *     if the operation is "copy") false.
   * @param {string=} opt_taskId If the corresponding item has already created
   *     at another places, we need to specify the ID of the item. If the
   *     item is not created, FileOperationManager generates new ID.
   */
  paste(sourceEntries, targetEntry, isMove, opt_taskId) {}

  /**
   * Returns true if all entries will use trash for delete.
   *
   * @param {!VolumeManager} volumeManager
   * @param {!Array<!Entry>} entries The entries.
   * @return {boolean}
   */
  willUseTrash(volumeManager, entries) {}

  /**
   * Schedules the files deletion.
   *
   * @param {!Array<!Entry>} entries The entries.
   */
  deleteEntries(entries) {}

  /**
   * Restores files from trash.
   *
   * @param {Array<!{name: string, filesEntry: !Entry, infoEntry: !FileEntry}>}
   *     trashEntries The trash entries.
   */
  restoreDeleted(trashEntries) {}

  /**
   * Creates a zip file for the selection of files.
   *
   * @param {!Array<!Entry>} selectionEntries The selected entries.
   * @param {!DirectoryEntry} dirEntry The directory containing the selection.
   */
  zipSelection(selectionEntries, dirEntry) {}

  /**
   * Generates new task ID.
   *
   * @return {string} New task ID.
   */
  generateTaskId() {}
};

window.ImportHistory = class {
  /**
   * @return {!Promise<!importer.ImportHistory>} Resolves when history
   *     has been fully loaded.
   */
  whenReady() {}

  /**
   * @param {!FileEntry} entry
   * @param {!importer.Destination} destination
   * @return {!Promise<boolean>} Resolves with true if the FileEntry
   *     was previously copied to the device.
   */
  wasCopied(entry, destination) {}

  /**
   * @param {!FileEntry} entry
   * @param {!importer.Destination} destination
   * @return {!Promise<boolean>} Resolves with true if the FileEntry
   *     was previously imported to the specified destination.
   */
  wasImported(entry, destination) {}

  /**
   * @param {!FileEntry} entry
   * @param {!importer.Destination} destination
   * @param {string} destinationUrl
   */
  markCopied(entry, destination, destinationUrl) {}

  /**
   * List urls of all files that are marked as copied, but not marked as synced.
   * @param {!importer.Destination} destination
   * @return {!Promise<!Array<string>>}
   */
  listUnimportedUrls(destination) {}

  /**
   * @param {!FileEntry} entry
   * @param {!importer.Destination} destination
   * @return {!Promise<?>} Resolves when the operation is completed.
   */
  markImported(entry, destination) {}

  /**
   * @param {string} destinationUrl
   * @return {!Promise<?>} Resolves when the operation is completed.
   */
  markImportedByUrl(destinationUrl) {}

  /**
   * Adds an observer, which will be notified when cloud import history changes.
   *
   * @param {!importer.ImportHistory.Observer} observer
   */
  addObserver(observer) {}

  /**
   * Remove a previously registered observer.
   *
   * @param {!importer.ImportHistory.Observer} observer
   */
  removeObserver(observer) {}
};

window.MediaScanner = class {
  /**
   * Initiates scanning.
   *
   * @param {!DirectoryEntry} directory
   * @param {!importer.ScanMode} mode
   * @return {!importer.ScanResult} ScanResult object representing the scan
   *     job both while in-progress and when completed.
   */
  scanDirectory(directory, mode) {}

  /**
   * Initiates scanning.
   *
   * @param {!Array<!FileEntry>} entries Must be non-empty, and all entries
   *     must be of a supported media type. Individually supplied files
   *     are not subject to deduplication.
   * @param {!importer.ScanMode} mode The method to detect new files.
   * @return {!importer.ScanResult} ScanResult object representing the scan
   *     job for the explicitly supplied entries.
   */
  scanFiles(entries, mode) {}

  /**
   * Adds an observer, which will be notified on scan events.
   *
   * @param {!importer.ScanObserver} observer
   */
  addObserver(observer) {}

  /**
   * Remove a previously registered observer.
   *
   * @param {!importer.ScanObserver} observer
   */
  removeObserver(observer) {}
};

/**
 * Class representing the results of an {importer} scan operation.
 *
 * @interface
 */
window.ScanResult = class {
  /**
   * @return {boolean} true if scanning is complete.
   */
  isFinal() {}

  /**
   * Notifies the scan to stop working. Some in progress work
   * may continue, but no new work will be undertaken.
   */
  cancel() {}

  /**
   * @return {boolean} True if the scan has been canceled. Some
   * work started prior to cancellation may still be ongoing.
   */
  canceled() {}

  /**
   * @param {number} count Sets the total number of candidate entries
   *     that were checked while scanning. Used when determining
   *     total progress.
   */
  setCandidateCount(count) {}

  /**
   * Event method called when a candidate has been processed.
   * @param {number} count
   */
  onCandidatesProcessed(count) {}

  /**
   * Returns all files entries discovered so far. The list will be
   * complete only after scanning has completed and {@code isFinal}
   * returns {@code true}.
   *
   * @return {!Array<!FileEntry>}
   */
  getFileEntries() {}

  /**
   * Returns all files entry duplicates discovered so far.
   * The list will be
   * complete only after scanning has completed and {@code isFinal}
   * returns {@code true}.
   *
   * Duplicates are files that were found during scanning,
   * where not found in import history, and were matched to
   * an existing entry either in the import destination, or
   * to another entry within the scan itself.
   *
   * @return {!Array<!FileEntry>}
   */
  getDuplicateFileEntries() {}

  /**
   * Returns a promise that fires when scanning is finished
   * normally or has been canceled.
   *
   * @return {!Promise<!importer.ScanResult>}
   */
  whenFinal() {}

  /**
   * @return {!importer.ScanResult.Statistics}
   */
  getStatistics() {}
};

window.MediaImportHandler = class {
  /**
   * @param {!ProgressCenter} progressCenter
   * @param {!importer.HistoryLoader} historyLoader
   * @param {!importer.DispositionChecker.CheckerFunction} dispositionChecker
   * @param {!DriveSyncHandler} driveSyncHandler
   */
  constructor(
      progressCenter, historyLoader, dispositionChecker, driveSyncHandler) {}

  importFromScanResult(scanResult, destination, directoryPromise) {}
};

/**
 * Provider of lazy loaded importer.ImportHistory. This is the main
 * access point for a fully prepared {@code importer.ImportHistory} object.
 */
window.HistoryLoader = class {
  /**
   * Instantiates an {@code importer.ImportHistory} object and manages any
   * necessary ongoing maintenance of the object with respect to
   * its external dependencies.
   *
   * @see importer.SynchronizedHistoryLoader for an example.
   *
   * @return {!Promise<!importer.ImportHistory>} Resolves when history instance
   *     is ready.
   */
  getHistory() {}

  /**
   * Adds a listener to be notified when history is fully loaded for the first
   * time. If history is already loaded...will be called immediately.
   *
   * @param {function(!importer.ImportHistory)} listener
   */
  addHistoryLoadedListener(listener) {}
};

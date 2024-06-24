chrome/browser/ash/chromebox_for_meetings/artemis
=================================================

Last modified: 04/12/2024

Overview
--------

This directory contains the source code for Artemis, a data-collection API
maintained by the Chromebox-for-Meetings team that supports aggregating
data from multiple sources and optionally uploading said data to a cloud
logging platform for developer analysis. The API also supports adding
"watchdogs" to any data source for action items that should be executed
on specific events.

Components
----------

- DataAggregator: responsible for fetching data from all sources and
  uploading data to cloud logging remote node.
- DataSource: represents a single source of data to be collected. Can be remote
  or local sources. Current supported (local) sources below:
    - LogSource: collects data from a single log file
    - CommandSource: collects output data from single command
- DataWatchDog: applied to a single DataSource. Executes a callback when
  chosen data source contains data that matches supplied filter.
- ERP node: remote endpoint that receives data using Chrome's Encrypted
  Reporting Pipeline protocol (see go/chromereportingdocs). This endpoint
  is managed by the Fleet team and will not be discussed here.

See mojom file for more detailed info: https://source.chromium.org/chromium/chromium/src/+/main:chromeos/services/chromebox_for_meetings/public/mojom/meet_devices_data_aggregator.mojom

Internal Notes
--------------

**DataAggregator**

- Manages multiple data sources
- Calls Fetch() on each DataSource on a periodic cadence. When the payload
  grows large enough, it will be pushed to the Fleet endpoint via ERP.
- If the upload succeeds, Flush() is called on corresponding DataSources
  to alert them that they can clear their internal buffers. If it fails,
  it will be re-attempted until success.

**DataSource**

- Collects data on its own (faster) cadence, separate from DataAggregator.
- Maintains an internal data buffer for new data. Calls to Fetch() will
  clear this data; the caller is responsible for ensuring that the data
  is transported properly.
- Internal buffer & separate cadence are used to support watchdogs. We want
  to poll for data much faster than the Fetch() cadence to ensure that (a)
  we trigger watchdog callbacks close to when the event occurs, and (b) we
  don't miss a pattern match altogether.
- If internal buffer is filled to its max limit, oldest entries will be
  purged to make room for new ones. This should only happen on repeated
  failures.

**LocalDataSource**

- An abstraction around a DataSource that serves data that can be obtained
  directly on the workstation
- Handles common operations, like internal buffer size capping, data redaction,
  watchdog validation, and upload preparation via structured data

**CommandSource**

- A type of LocalDataSource that collects output data from supplied command
- Internal buffer will only be appended to if the current output is not equal
  to the last-appended data, OR if a watchdog is added
- Supports both CHANGE and REGEX watchdogs

**LogSource**

- A type of LocalDataSource that collects output data from supplied log file
- Internal buffer will collect a chunk (N lines) from the file on each
  iteration
- Due to the nature of the data, LogSources only support REGEX watchdogs

Helpful Links
-------------

- Latest design doc: go/cfm-lacros-feedback

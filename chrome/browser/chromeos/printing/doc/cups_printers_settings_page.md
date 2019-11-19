# CUPS Printers Settings Page

The CUPS Printers settings page (located at `chrome://settings/cupsPrinters`)
displays the the currently configured native printers and allows users to set up
their own native printers using CUPS.

The Chrome client code which is responsible for handling UI events from this
page is located at
`chrome/browser/ui/webui/settings/chromeos/cups_printers_handler.cc`

The front-end code for this page is located at
`chrome/browser/resources/settings/printing_page/`

## Add Printer Dialogs

There are 4 dialogs that are related to adding a printer. The code for these
dialogs is located at `cups_add_printer_dialog.js`

### Discovered Printers Dialog

`add-printer-discovery-dialog`

Displays discovered network and USB printers which are available for setup.

### Manual Dialog

`add-printer-manually-dialog`

Allows users to manually enter the information of a new printer to be added.

### Configuring Dialog

`add-printer-configuring-dialog`

This dialog is used to indicate to a user that their add printer operation is
currently in progress.

### Manufacturer Model Dialog

`add-printer-manufacturer-model-dialog`

This dialog allows the user to select the manufacturer and model of the printer
that they are attempting to add. Each manufacturer/model combination corresponds
to a PPD file, so this dialog is used to select a PPD file for the printer.
There is also the option for the user to provide their own PPD file.

This dialog should only be shown in cases where we were unable to match a PPD to
the printer that a user attempted to add.

## Edit Printer Dialog

`settings-cups-edit-printer-dialog`

Allows the user to edit an existing configured printer.

The code for this dialog is located at `cups_edit_printer_dialog.js`

The following fields of the printer may be changed:

*   **Name**: The name of the printer displayed in the UI.
*   **Address**: The "hostname" of the printer. This can be a valid hostname,
    IPv4/6 address, and may be followed by an optional port number. This field
    can only be edited if the existing printer is already a network printer.
*   **Protocol**: The protocol used in the printer's URI. If the existing
    printer is a network printer then this may only be changed to another
    network protocol. Otherwise the protocol may not be changed at all.
*   **Queue**: The "path" which the address points to. For ipp-based URIs this
    is typically ipp/print.
*   **Manufacturer**: The manufacturer of the printer, if this field is changed
    then the **Model** field must be changed as well.
*   **Model**: The model name of a printer, this name corresponds to a PPD.
*   **User PPD**: The user-specified PPD.

If any field other than **Name** is changed on the existing printer, the
revised printer object is reconfigured using the add printer flow.

## CUPS Browser Proxy
The CUPS browser proxy is responsible for sending messages from the UI in the
settings page to the client code and retrieving results if necessary.

The code for the browser proxy is located at
`/printing_page/cups_printers_browser_proxy.js`

The following table contains message names as they appear in
`cups_printers_browser_proxy.js` and their corresponding functions in
`cups_printers_handler.cc`:

Message Name                        | Function
:---------------------------------- | :-------
`getCupsPrintersList`               | `HandleGetCupsPrintersList`
`updateCupsPrinter`                 | `HandleUpdateCupsPrinter`
`removeCupsPrinter`                 | `HandleRemoveCupsPrinter`
`addCupsPrinter`                    | `HandleAddCupsPrinter`
`getPrinterInfo`                    | `HandleGetPrinterInfo`
`getCupsPrinterManufacurersList`    | `HandleGetCupsPrinterManufacturers`
`getCupsPrintersModelList`          | `HandleGetCupsPrintersModels`
`selectPPDFile`                     | `HandleSelectPPDFile`
`startDiscoveringPrinters`          | `HandleStartDiscovery`
`stopDiscoveringPrinters`           | `HandleStopDiscovery`
`getPrinterPpdManufacturerAndModel` | `HandleGetPrinterPpdManufacturerAndModel`
`addDiscoveredPrinter`              | `HandleAddDiscoveredPrinter`
`cancelPrinterSetup`                | `HandleSetUpCancel`
`getEulaUrl`                        | `HandleGetEulaUrl`

## Javascript Listeners

The following tables contain the different event listeners that exist within the
frontend UI code and their corresponding event handler functions.

### `cups_add_printer_dialog.js`
Event Name                           | Event Handler
:----------------------------------- | :--------------------------------
`on-printer-discovered`              | `onPrinterDiscovered_`
`on-printer-discovery-done`          | `onPrinterDiscoveryDone_`
`on-add-cups-printer`                | `onAddPrinter_`
`on-manually-add-discovered-printer` | `onManuallyAddDiscoveredPrinter_`
`configuring-dialog-closed`          | `ConfiguringDialogClosed_`
`open-manually-add-printer`          | `openManuallyAddPrinterDialog_`
`open-configuring-printer-dialog`    | `openConfiguringPrinterDialog_`
`open-discovery-printers-dialog`     | `openDiscoveryPrintersDialog_`
`open-manufacturer-model-dialog`     | `openManufacturerModelDialog_`
`no-detected-printer`                | `onNoDetectedPrinter_`

### `cups_printers.js`

Event Name                  | Event Handler
:-------------------------- | :-----------------------------
`edit-cups-printer-details` | `onShowCupsEditPrinterDialog_`
`on-add-cups-printer`       | `onAddPrinter_`
`on-printer-changed`        | `printerChanged_`

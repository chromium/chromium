# ChromeOS Printing

This directory contains browser-side code for printing infrastructure in
ChromeOS.  This directory primarily contains code dealing with local printing
via the Common Unix Printing System (CUPS).

## Other Related Directories

(Paths are given from the git root):

* `chromeos/printing/` - ChromeOS CUPS printing code that doesn't have
  dependencies that require it to live in chrome/browser.
* `chrome/browser/ui/webui/settings/ash/` - ChromeOS printing settings
  dialog backend support
* `chrome/browser/resources/settings/printing_page/` - Front end printer
  settings code.
* `chrome/browser/printing/` - Cloud print support, and common print dialog
  support.
* `chrome/browser/ash/printing/` - ChromeOS printing code that runs in
  ash-chrome.

## Printing Docs

* [Cups Printer Management](cups_printer_management.md) - Overview of how CUPS
  printers are managed in ChromeOS.
* [Cups Printers Settings Page](cups_printers_settings_page.md) - Contains
  information about the various dialogs, pages, and various messages/listeners
  in the UI.

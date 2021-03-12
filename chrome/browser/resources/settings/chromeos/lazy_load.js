// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Uncomment as these modules are migrated to Polymer 3.
import './crostini_page/crostini_arc_adb.m.js';
import './crostini_page/crostini_arc_adb_confirmation_dialog.m.js';
import './crostini_page/crostini_disk_resize_confirmation_dialog.m.js';
import './crostini_page/crostini_disk_resize_dialog.m.js';
import './crostini_page/crostini_export_import.m.js';
import './crostini_page/crostini_import_confirmation_dialog.m.js';
import './crostini_page/crostini_mic_sharing_dialog.m.js';
import './crostini_page/crostini_page.m.js';
import './crostini_page/crostini_port_forwarding.m.js';
import './crostini_page/crostini_port_forwarding_add_port_dialog.m.js';
import './crostini_page/crostini_subpage.m.js';
import './date_time_page/date_time_page.m.js';
import './date_time_page/timezone_selector.m.js';
import './guest_os/guest_os_shared_usb_devices.m.js';
import './guest_os/guest_os_shared_paths.m.js';
import './os_a11y_page/os_a11y_page.m.js';
import './os_a11y_page/manage_a11y_page.m.js';
import './os_a11y_page/switch_access_action_assignment_dialog.m.js';
import './os_a11y_page/switch_access_setup_guide_dialog.m.js';
import './os_a11y_page/switch_access_subpage.m.js';
import './os_a11y_page/tts_subpage.m.js';
import './os_files_page/os_files_page.m.js';
import './os_languages_page/input_method_options_page.m.js';
import './os_languages_page/input_page.m.js';
import './os_languages_page/os_edit_dictionary_page.m.js';
import './os_languages_page/os_languages_page.m.js';
import './os_languages_page/os_languages_page_v2.m.js';
import './os_languages_page/os_languages_section.m.js';
import './os_languages_page/smart_inputs_page.m.js';
import './os_printing_page/cups_add_print_server_dialog.m.js';
import './os_printing_page/cups_add_printer_dialog.m.js';
import './os_printing_page/cups_add_printer_manually_dialog.m.js';
import './os_printing_page/cups_add_printer_manufacturer_model_dialog.m.js';
import './os_printing_page/cups_edit_printer_dialog.m.js';
import './os_printing_page/cups_nearby_printers.m.js';
import './os_printing_page/cups_printer_dialog_error.m.js';
import './os_printing_page/cups_printer_shared_css.m.js';
import './os_printing_page/cups_printers_entry.m.js';
import './os_printing_page/cups_printers.m.js';
import './os_printing_page/cups_saved_printers.m.js';
import './os_printing_page/cups_settings_add_printer_dialog.m.js';
import './os_printing_page/os_printing_page.m.js';
import './os_privacy_page/os_privacy_page.m.js';
import './os_privacy_page/peripheral_data_access_protection_dialog.m.js';
import './os_reset_page/os_reset_page.m.js';
import './os_reset_page/os_powerwash_dialog.m.js';
import './os_reset_page/os_reset_page.m.js';
import './os_files_page/smb_shares_page.m.js';

export {SmbBrowserProxyImpl, SmbMountResult} from 'chrome://resources/cr_components/chromeos/smb_shares/smb_browser_proxy.m.js';
export {LanguagesBrowserProxy, LanguagesBrowserProxyImpl} from '../languages_page/languages_browser_proxy.js';
export {LifetimeBrowserProxyImpl} from '../lifetime_browser_proxy.js';
export {CrostiniBrowserProxy, CrostiniBrowserProxyImpl} from './crostini_page/crostini_browser_proxy.m.js';
export {TimeZoneAutoDetectMethod} from './date_time_page/date_time_types.m.js';
export {TimeZoneBrowserProxyImpl} from './date_time_page/timezone_browser_proxy.m.js';
export {CROSTINI_TYPE, GuestOsBrowserProxy, GuestOsBrowserProxyImpl, GuestOsSharedUsbDevice, PLUGIN_VM_TYPE} from './guest_os/guest_os_browser_proxy.m.js';
export {LanguagesMetricsProxy, LanguagesMetricsProxyImpl, LanguagesPageInteraction} from './os_languages_page/languages_metrics_proxy.m.js';
export {PrinterType} from './os_printing_page/cups_printer_types.m.js';
export {CupsPrintersBrowserProxy, CupsPrintersBrowserProxyImpl, PrinterSetupResult, PrintServerResult} from './os_printing_page/cups_printers_browser_proxy.m.js';
export {CupsPrintersEntryManager} from './os_printing_page/cups_printers_entry_manager.m.js';
export {PeripheralDataAccessBrowserProxy, PeripheralDataAccessBrowserProxyImpl} from './os_privacy_page/peripheral_data_access_browser_proxy.m.js';
export {OsResetBrowserProxyImpl} from './os_reset_page/os_reset_browser_proxy.m.js';

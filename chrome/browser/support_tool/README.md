# Support Tool

Support Tool is a framework that collects various information and logs from
ChromeOS devices and Chrome browser. The caller can use it to gather the
desired set of logs using
[SupportToolHandler](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/support_tool/support_tool_handler.h).
See the documentation there on the usage of it.

### Local Usage

Use Support Tool UI on chrome://support-tool to export the log archive to local
storage.

### Remote Log Collection for Enterprise Admins

Admins can trigger log collection on Admin Console to get the logs remotely.
See [here](https://support.google.com/chrome/a?p=remote-log) for more details.

## Available Data on Support Tool

| Name                                                                                | Description                                                                                                                                         |
|-------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------|
| [ChromeOS Flex Logs](#chromeos-flex-logs)                                           | Collect Hardware data for cloudready devices via cros_healthd calls.                                                                                |
| [UI Hierarchy](#ui-hierarchy)                                                       | Fetches UI component hierarchy for ChromeOS.                                                                                                        |
| [Crash IDs](#crash-ids)                                                             | Extracts the most recent crash IDs (if any) and exports them into crash_report_ids and all_crash_report_ids files.                                  |
| [Chrome System Information](#chrome-system-information)                             | Fetches internal Chrome logs.                                                                                                                       |
| [Lacros](#lacros)                                                                   | Gets Lacros system information log data if Lacros is running.                                                                                       |
| [Lacros System Information](#lacros-system-information)                             | Gets the crosapi version that supports the Lacros remote data source if Lacros is running.                                                          |
| [Intel WiFi NICs Debug Dump](#intel-wifi-nics-debug-dump)                           | Fetches debug dump information from Intel Wi-Fi NICs that will be produced when those NICs have issues such as firmware crashes for ChromeOS.       |
| [ChromeOS Network Routes](#chromeos-network-routes)                                 | Gathers network routing tables for ipv4 and ipv6                                                                                                    |
| [Additional ChromeOS Platform Logs](#additional-chromeos-platform-logs)             | Gathers log data from various scripts/programs. Creates and exports data into these files: alsa controls, cras, audio_diagnostics, env, disk_usage. |
| [Touch Events](#touch-events)                                                       | Fetches touch events, touchscreen and touchpad logs.                                                                                                |
| [DBus Details](#dbus-details)                                                       | Fetches memory usage details of DBus interface. Creates and exports data into these files: dbus_details, dbus_summary.                              |
| [Device Event](#device-event)                                                       | Fetches entries for 'network_event_log' and 'device_event_log'.                                                                                     |
| [Memory Details](#memory-details)                                                   | Fetches memory usage details and exports them into mem_usage and mem_usage_with_title files.                                                        |
| [Policies](#policies)                                                               | Policies applied for device or user for managed users.                                                                                              |
| [ChromeOS Shill (Connection Manager) Logs](#chromeos-shill-connection-manager-logs) | Gathers Device and Service properties from Shill.                                                                                                   |
| [ChromeOS System Logs](#chromeos-system-logs)                                       | Gathers the contents of /var/log file. /var/log file contains the log files of various system files e.g. Chrome logs, messages, bluetooth logs etc. |
| [ChromeOS System State and Logs](#chromeos-system-state-and-logs)                   | Gathers log data from Debug Daemon. Debug daemon collects log from several system files or runs scripts to collect data.                            |
| [ChromeOS Chrome User Logs](#chromeos-chrome-user-logs)                             | Gathers logs from user's directory on ChromeOS. Contains Chrome logs, log-in/log-out times and Google Assistant logs                                |
| [ChromeOS Bluetooth](#chromeos-bluetooth)                                           | Fetches if Bluetooth floss is enabled on the device                                                                                                 |
| [ChromeOS Connected Input Devices](#chromeos-connected-input-devices)               | Fetches the information about connected input devices to ChromeOS device.                                                                           |
| [ChromeOS Virtual Keyboard](#chromeos-virtual-keyboard)                             | Fetches the virtual keyboard details on ChromeOS                                                                                                    |
| [ChromeOS Traffic Counters](#chromeos-traffic-counters)                             | Fetches traffic counters for ChromeOS                                                                                                               |
| [ChromeOS Network Health](#chromeos-network-health)                                 | Fetches network health entry.                                                                                                                       |
| [Performance and Battery Status](#performance-and-battery-status)                   | Gathers performance relevant data such as battery saving mode on device or the active battery status                                                |
| [Sign In Status](#sign-in-status)                                                   | Fetches signin tokens and details (the information on chrome://signin-internals)                                                                    |
| [ChromeOS App Service](#chromeos-app-service)                                       | Gathers information from app service about installed and running apps                                                                               |


## Details About Available Data

### ChromeOS Flex Logs

Collect hardware data for ChromeOS Flex devices via cros_healthd calls. What
does the information contain:

-   System Info
    -   Vendor (chromeosflex_product_vendor)
    -   Product name (chromeosflex_product_name)
    -   Product version (chromeosflex_product_version)
    -   Bios name (chromeosflex_bios_version)
    -   Secure boot enabled (chromeosflex_secureboot)
    -   UEFI enabled (chromeosflex_uefi)
-   Cpu Info
    -   CPU names under name (chromeosflex_cpu_name)
-   Memory Info
    -   Total memory (chromeosflex_total_memory)
    -   Free memory (chromeosflex_free_memory)
    -   Available memory (chromeosflex_available_memory)
-   Bus Devices Info
    -   Ethernet devices
        -   name (chromeosflex_ethernet_name)
        -   id (chromeosflex_ethernet_id)
        -   driver (chromeosflex_ethernet_driver)
    -   Bluetooth devices
        -   name (chromeosflex_bluetooth_name)
        -   id (chromeosflex_bluetooth_id)
        -   driver (chromeosflex_bluetooth_driver)
    -   Wireless devices
        -   name (chromeosflex_wireless_name)
        -   id (chromeosflex_wireless_id)
        -   driver (chromeosflex_wireless_driver)
    -   GPU devices
        -   name (chromeosflex_gpu_name)
        -   id (chromeosflex_gpu_id)
        -   driver (chromeosflex_gpu_driver)

-   Tpm Info
    -   (chromeosflex_tpm_allow_listed)
    -   (chromeosflex_tpm_did_vid)
    -   (chromeosflex_tpm_manufacturer)
    -   (chromeosflex_tpm_owned)
    -   (chromeosflex_tpm_spec_level)
    -   (chromeosflex_tpm_version)
-   Graphics Info
    -   chromeosflex_gl_extensions
    -   chromeosflex_gl_renderer
    -   chromeosflex_gl_shading_version
    -   chromeosflex_gl_vendor
    -   chromeosflex_gl_version
-   Touchpad library name

Source code on [RevenLogSource](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/system_logs/reven_log_source.h).

### UI Hierarchy

UI component hierarchy.

-   Windows
-   Layers
-   Views

Source code on [UiHierarchyDataCollector](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/support_tool/ash/ui_hierarchy_data_collector.h).

### Crash IDs

-   crash_report_ids
-   all_crash_report_ids

Source code on [CrashIdsSource](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/feedback/system_logs/log_sources/crash_ids_source.h).


### Chrome System Information

-   All devices: (including Win, Mac):
    -   Sync logs
        -   The sync data that can be available in chrome://sync-internals in
        JSON format
    -   Extension info logs
        -   List in format: "extension_id" : "extension_name" : "extension_version"
    -   Power API logs
        -   The extension levels from chrome.power API
    -   Chrome version (incl. Lacros version if enabled)
    -   Enrollment status
    -   OS version for ChromeOS
    -   CPU arc for Windows and Mac
    -   Is skia graphite enabled?
    -   Is child account?
-   ChromeOS:
    -   Local state settings
    -   Arc policy status if ARC is enabled
    -   Onboarding time:
        -   Time when a new user has finished onboarding.
    -   Account type
    -   Lacros status (enabled/disabled)
    -   Demo mode config
    -   Failed knowledge factor events
    -   Recorded auth events
    -   Monitor (display) information
    -   Disk space:
        -   Free disk space
        -   Total disk space

-   Windows:
    -   USB keyboard added
    -   Enrollment status
    -   Installer brand
    -   Last update state

Source code on [ChromeInternalLogSource](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/feedback/system_logs/log_sources/chrome_internal_log_source.h).


### Lacros

Lacros logs on the device:

-   lacros.log
-   lacros.previous

Source code on [LacrosLogFilesLogSource](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/feedback/system_logs/log_sources/lacros_log_files_log_source.h).


### Lacros System Information

-   Gets information for Lacros browser through CrosAPI calls. It contains entries such as:
    -   Contents of "Chrome System Information" for Lacros
    -   Crash IDs for Lacros browser
    -   Device events
    -   Memory Details
    -   Ozone Wayland State Dump

Source code on [CrosapiSystemLogSource](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/system_logs/crosapi_system_log_source.h).


### Intel WiFi NICs Debug Dump

Retrieves contents of `/var/log/last_iwlwifi_dump` file.

Source code on [IwlWifiDumpLogSource](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/system_logs/iwlwifi_dump_log_source.h).


### ChromeOS Network Routes

Returns the network routes for ipv4 and ipv6 (through debugd).

Source code on [NetworkRoutesDataCollector](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/support_tool/ash/network_routes_data_collector.h).

### Additional ChromeOS Platform Logs

-   alsa controls
-   cras
-   audio_diagnostics
-   env
-   system_files
-   disk_usage

Source code on [CommandLineLogSource](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/system_logs/command_line_log_source.h).

### Touch Events

Touch event logs and touch device state logs from [Input controller](https://source.chromium.org/chromium/chromium/src/+/main:ui/ozone/public/input_controller.h).

Source code on [TouchLogSource](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/system_logs/touch_log_source.h).


### DBus Details

DBus call statistics: contains names for DBus methods and paths.

Source code on [DBusLogSource](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/system_logs/dbus_log_source.h).


### Device Event

-   "network_event_log": Network events that occurred on the device.
-   "device_event_log": All other device events.

See UI on chrome://device-log.

Source code on [DeviceEventLogSource](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/feedback/system_logs/log_sources/device_event_log_source.h).


### Memory Details

mem_usage: the memory usage of the browser process and its subprocesses.

Source code on [MemoryDetailsLogSource](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/feedback/system_logs/log_sources/memory_details_log_source.h).


### Policies

Policies and their values. Contains the policy status and last fetch time. UI
available on chrome://policy.

Source code on [PolicyDataCollector](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/support_tool/policy_data_collector.h).


### ChromeOS Shill (Connection Manager) Logs

TODO: b/308088383 - Add details.

Source code on [ShillDataCollector](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/support_tool/ash/shill_data_collector.h).


### ChromeOS System Logs

TODO: b/308088383 - Add details.

Source code on [SystemLogsDataCollector](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/support_tool/ash/system_logs_data_collector.h).


### ChromeOS System State and Logs

TODO: b/308088383 - Add details.

Source code on [SystemStateDataCollector](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/support_tool/ash/system_state_data_collector.h).


### ChromeOS Chrome User Logs

-   Chrome browser logs from user session
-   Assistant logs
-   Login/logout times

Source code on [ChromeUserLogsDataCollector](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/support_tool/ash/chrome_user_logs_data_collector.h).


### ChromeOS Bluetooth

TODO: b/308088383 - Add details.

Source code on [BluetoothDataCollector](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/system_logs/bluetooth_log_source.h).


### ChromeOS Connected Input Devices

TODO: b/308088383 - Add details.

Source code on [ConnectedInputDevicesLogSource](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/system_logs/connected_input_devices_log_source.h).


### ChromeOS Virtual Keyboard

TODO: b/308088383 - Add details.

Source code on [VirtualKeyboardLogSource](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/system_logs/virtual_keyboard_log_source.h).


### ChromeOS Traffic Counters

TODO: b/308088383 - Add details.

Source code on [TrafficCountersLogSource](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/system_logs/traffic_counters_log_source.h).


### ChromeOS Network Health

TODO: b/308088383 - Add details.

Source code on [NetworkHeathDataCollector](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/support_tool/ash/network_health_data_collector.h).


### Performance and Battery Status

-   high_efficiency_mode_active
-   battery_saver_state
-   battery_saver_mode_active
-   battery_saver_disabled_for_session
-   device_has_battery
-   device_using_battery_power
-   device_battery_percentage

Source code on [PerformanceLogSource](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/feedback/system_logs/log_sources/performance_log_source.h).


### Sign In Status

TODO: b/308088383 - Add details.

Source code on [SigninDataCollector](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/support_tool/signin_data_collector.h).


### ChromeOS App Service

TODO: b/308088383 - Add details.

Source code on [AppServiceLogSource](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/system_logs/app_service_log_source.h).

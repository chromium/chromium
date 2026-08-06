# Generating and Applying Custom Orderfiles and PGO for WebView in AOSP

[TOC]

## Background

This document provides instructions for AOSP system integrators on how to
generate and apply custom Orderfile and Profile-Guided Optimization (PGO)
profiles for WebView builds.

> [!NOTE]
> The tools described in this document do not support x86 or x86-64 architectures.
> You will need to use an ARM or ARM64 environment (such as a physical device)
> rather than an x86 emulator.

### Orderfile
An orderfile is a list of symbols that defines a specific ordering of
functions. A static linker, such as LLD, can follow this ordering when
generating a binary to optimize performance. Reordering code improves startup
and page load performance by fetching machine code into memory more efficiently.
The Orderfile can be generated for both **32-bit (arm)** and **64-bit (arm64)**
architectures.

### Profile-Guided Optimization (PGO)
PGO is a compiler optimization technique that uses profile data collected
from representative runs of an application to make better optimization
decisions (e.g., inlining, branch prediction).

On Android, PGO is currently **only applied to the 64-bit WebView library**.
The 32-bit library uses AutoFDO (AFDO), which is not covered in this document.

For more general information, see the documentation for
[Orderfiles](/docs/orderfile.md) and [PGO](/docs/pgo.md).

## Generating Custom PGO Profiles

To generate a custom PGO profile, follow these steps:

### 1. Update your `.gclient` configuration

Ensure that your `.gclient` file includes the following custom variables:

```python
'custom_vars': {
    'checkout_pgo_profiles': True,
    'checkout_telemetry_dependencies': True,
},
```

Run `gclient sync` to fetch the necessary tools.

### 2. Configure GN Arguments

Create a build directory (e.g., `out/pgo-generate`) and configure the GN
arguments for the instrumentation phase.

```gn
chrome_pgo_phase = 1
clang_use_default_sample_profile = false
debuggable_apks = false
is_official_build = true
symbol_level = 1
target_cpu = "arm64"
target_os = "android"
v8_is_on_release_branch = true
```

### 3. Build and Run the Generation Script

Build the generation target and run the script to collect profile data.

```bash
autoninja -C out/pgo-generate/ tools/pgo:generate_profile_android_webview_64
cd out/pgo-generate/
bin/run_generate_profile_android_webview_64 -vv
```

This script will run the profiling scenarios on a connected Android device
and produce the profile data.

### 4. Retrieve the Output

The generated PGO profile will be located at `out/pgo-generate/profile.profdata`.

### Customizing the PGO Workload

The default PGO generation pipeline uses specific scenarios that require an
internal checkout. **Non-Googler readers should expect to need to perform
modifications** to the workload or exercise different user journeys. To
customize the workload, you should inspect the following script:

*   [`tools/pgo/generate_profile_webview.py`](/tools/pgo/generate_profile_webview.py)

Modifying this script allows you to change the scenarios being exercised.

## Generating Custom Orderfiles Manually

Generating a custom orderfile is useful if you want to optimize for specific
use cases.

> [!IMPORTANT]
> **Orderfile and PGO Coupling**: For the best results on 64-bit builds, you
> should generate the PGO profile first, and then generate the Orderfile
> while the custom PGO profile is applied. See the
> [PGO documentation](/docs/pgo.md#interaction-with-orderfile-generation)
> for more context.

### 1. Update your `.gclient` configuration

Ensure that your `.gclient` file includes the following custom variables:

```python
'custom_vars': {
    'checkout_pgo_profiles': True,
    'checkout_telemetry_dependencies': True,
},
```

Run `gclient sync` to fetch the necessary tools.

### 2. Configure GN Arguments

Create a build directory (e.g., `out/orderfile-generate`) and configure the
GN arguments. Use the following arguments as a baseline, setting
`target_cpu` to `"arm64"` or `"arm"` depending on your target architecture.

If you generated a custom PGO profile for an **arm64** build, you should
apply it here by setting `pgo_override_filename` to your generated profile
file name after copying it to the appropriate directory (see
[Applying PGO](#applying-pgo-64-bit-only)).

```gn
debuggable_apks = false
enable_proguard_obfuscation = false
is_chrome_branded = false
is_official_build = true
symbol_level = 1
target_cpu = "arm64" # or "arm"
target_os = "android"
use_order_profiling = true
# Optional (arm64 only): Name of your custom PGO profile
# pgo_override_filename = "my_profile.profdata"
```

### 3. Build the Target

Build the generation target using `autoninja`.

*   **For arm64:**
    ```bash
    autoninja -C out/orderfile-generate/ \
        tools/cygprofile:generate_orderfile_android_webview_64
    ```
*   **For arm (32-bit):**
    ```bash
    autoninja -C out/orderfile-generate/ \
        tools/cygprofile:generate_orderfile_android_webview
    ```

### 4. Run the Generation Script

Run the generated script to execute the profiling scenarios and generate the
orderfile. You will need an Android device connected via `adb`.

*   **For arm64:**
    ```bash
    cd out/orderfile-generate && bin/run_generate_orderfile_android_webview_64
    ```
*   **For arm (32-bit):**
    ```bash
    cd out/orderfile-generate && bin/run_generate_orderfile_android_webview
    ```

This script installs the instrumented WebView on your device, sets it as the
active provider, exercises the defined user journeys, collects the profiles,
and generates the final orderfile.

### 5. Retrieve the Output

The generated orderfile will be located in the
`out/orderfile-generate/orderfiles/orderfile.arm64.out` for arm64 and in
`out/orderfile-generate/orderfiles/orderfile.arm.out` for arm.

### Customizing the Orderfile Workload

The default orderfile generation pipeline uses specific scenarios that
require an internal checkout. **Non-Googler readers should expect to need to
perform modifications** to the workload or exercise different user journeys. To
customize the workload, you should inspect the following script and
surrounding utility files:

*   [`tools/cygprofile/generate_orderfile.py`](/tools/cygprofile/generate_orderfile.py)

For the exact logic of generating the WebView profile, you can also refer to:

*   [`tools/cygprofile/android_profile_tool.py`](/tools/cygprofile/android_profile_tool.py)

Modifying these scripts allows you to tailor the generation process to
match the specific usage patterns required for your system integration.

## Applying Optimizations to an Official Build

Once you have generated your custom profiles, you can apply them to an
official WebView build. Follow the general instructions in the
[WebView for AOSP system integrators](aosp-system-integration.md) guide,
and add the following GN arguments:

### Applying PGO (64-bit only)

To apply the generated PGO profile to an official build:

1.  Copy your generated profile file (e.g., `my_profile.profdata`) into the
    `//chrome/build/pgo_profiles/` directory.
2.  Set the `pgo_override_filename` GN argument in your official build
    configuration to the file name:

```gn
pgo_override_filename = "my_profile.profdata"
```

### Applying Orderfile
Add the `webview_orderfile_path` argument to override the orderfile for
**both** the 32-bit and 64-bit libraries:

```gn
webview_orderfile_path = "//path/to/your/custom_orderfile.out"
```

You can combine both arguments in your official build configuration for
maximum optimization on the 64-bit library.

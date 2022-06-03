# Incremental Install

Incremental Install is a way of building & deploying an APK that tries to
minimize the time it takes to make a change and see that change running on
device. They work best with `is_component_build=true`, and do *not* require a
rooted device.

## Building

Add the gn arg:

    incremental_install = true

This causes all apks to be built as incremental except for denylisted ones.

## Running

It is not enough to `adb install` them. You must use the generated wrapper
script:

    out/Debug/bin/your_apk run
    out/Debug/bin/run_chrome_public_test_apk  # Automatically sets --fast-local-dev

# How it Works

## Overview

The basic idea is to sideload .dex and .so files to `/data/local/tmp` rather
than bundling them in the .apk. Then, when making a change, only the changed
.dex / .so needs to be pushed to the device.

Faster Builds:

 * No `final_dex` step (where all .dex files are merged into one)
 * No need to rebuild .apk for code-only changes (but required for resources)
 * Apks sign faster because they are smaller.

Faster Installs:

 * The .apk is smaller, and so faster to verify.
 * No need to run `adb install` for code-only changes.
 * Only changed .so / .dex files are pushed. MD5s of existing on-device files
   are cached on host computer.

Slower Initial Runs:

 * The first time you run an incremental .apk, the `DexOpt` needs to run on all
   .dex files. This step is normally done during `adb install`, but is done on
   start-up for incremental apks.
   * DexOpt results are cached, so subsequent runs are faster.
   * The slowdown varies significantly based on the Android version. Android O+
     has almost no visible slow-down.

Caveats:
 * Isolated processes (on L+) are incompatible with incremental install. As a
   work-around, isolated processes are disabled when building incremental apks.
 * Android resources, assets, and `loadable_modules` are not sideloaded (they
   remain in the apk), so builds & installs that modify any of these are not as
   fast as those that modify only .java / .cc.
 * Since files are sideloaded to `/data/local/tmp`, you need to use the wrapper
   scripts to uninstall them fully. E.g.:
   ```shell
   out/Default/bin/chrome_public_apk uninstall
   ```

## The Code

All incremental apks have the same classes.dex, which is built from:

    //build/android/incremental_install:bootstrap_java

They also have a transformed `AndroidManifest.xml`, which overrides the the
main application class and any instrumentation classes so that they instead
point to `BootstrapApplication`. This is built by:

    //build/android/incremental_install/generate_android_manifest.py

Wrapper scripts and install logic is contained in:

    //build/android/incremental_install/create_install_script.py
    //build/android/incremental_install/installer.py

Finally, GN logic for incremental apks is sprinkled throughout.

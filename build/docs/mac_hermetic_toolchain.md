# Mac and iOS hermetic toolchain instructions

The following is a short explanation of why we use a the hermetic toolchain
and instructions on how to roll a new toolchain. This toolchain is only
available to Googlers and infra bots.

## How to roll a new hermetic toolchain.

1. Download a new version of Xcode, and confirm either mac or ios builds
   properly with this new version.

2. Create a new CIPD package by moving Xcode.app to the `build/` directory, then
   follow the instructions in
   [build/xcode_binaries.yaml](../xcode_binaries.yaml).

   The CIPD package creates a subset of the toolchain necessary for a build.

2. Create a CL with the updated `MAC_BINARIES_TAG` in 
   [mac_toolchain.py](../mac_toolchain.py) with the version created by the
   previous command.

3. Run the CL through the trybots to confirm the roll works.

## Why we use a hermetic toolchain.

Building Chrome Mac currently requires many binaries that come bundled with
Xcode, as well the macOS and iphoneOS SDK (also bundled with Xcode). Note that
Chrome ships its own version of clang (compiler), but is dependent on Xcode
for these other binaries. Using a hermetic toolchain has two main benefits:

1. Build Chrome with a well-defined toolchain (rather than whatever happens to
   be installed on the machine).

2. Easily roll/update the toolchain.

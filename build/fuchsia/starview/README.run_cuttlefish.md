# Running Cuttlefish raw QEMU Locally for Fuchsia Starnix

This directory contains the launcher tools for booting Android Cuttlefish system images inside raw QEMU. This is used for testing Fuchsia Starnix compatibility.

The launcher script [run_cuttlefish.py](file://./run_cuttlefish.py) automates the virtual disk assembly, network card topology slot mapping, serial console Mock HVC responder setups, and ADB connection forwarding.

---

## Prerequisites

Before running the emulator, make sure your local workstation is set up:

1. **gLinux Setup**:
   * You need to have permissions to run KVM virtualization.
   * Add your user to the `kvm` and `render` groups:
     ```bash
     sudo usermod -aG kvm $USER
     sudo usermod -aG render $USER
     ```
   * Log out and log back in, or run `exec su - $USER` to refresh your shell groups membership.

---

## Downloading Prebuilts Manually

To run the emulator locally, you must first download the required system image and bootloader prebuilts. You can fetch them from internal Android Build servers using `/google/data/ro/projects/android/fetch_artifact` (requires `gcert` LOAS credentials):

1. **Create the local packages directories**:
   ```bash
   mkdir -p /path/to/packages/cuttlefish /path/to/packages/uboot
   ```

2. **Download the Cuttlefish Guest Image ZIP**:
   Specify the Android Build ID (e.g. `15680578`):
   ```bash
   /google/data/ro/projects/android/fetch_artifact \
     --bid 15680578 \
     --target cf_x86_64_blazer_starnix-trunk_staging-userdebug-byob \
     "cf_x86_64_blazer_starnix-img-*.zip" \
     /path/to/packages/cuttlefish/
   ```

3. **Download the QEMU U-Boot Bootloader**:
   Fetch the latest U-Boot BIOS rom:
   ```bash
   /google/data/ro/projects/android/fetch_artifact \
     --latest \
     --branch aosp_u-boot-mainline \
     --target u-boot_qemu_x86_64 \
     u-boot.rom \
     /path/to/packages/uboot/
   ```

4. **Install simg2img (if not already present in PATH)**:
   The script needs the `simg2img` tool to unsparse the Android super image.
   On gLinux/Debian/Ubuntu, you can install it via:
   ```bash
   sudo apt-get install android-sdk-libsparse-utils
   ```
   Alternatively, you can download/place a prebuilt `simg2img` binary under the packages directory:
   `/path/to/packages/simg2img/bin/simg2img`

---

## How to Run

To start the emulator, execute the boot script from the Chromium `src/` directory root, pointing it to the directory containing your prebuilt packages:

```bash
python3 build/fuchsia/starview/run_cuttlefish.py --packages /path/to/packages/
```

### Useful Command Line Arguments

* `--packages <dir>`: Directory containing:
  * `cuttlefish/` subdirectory: Must contain the AOSP guest image ZIP file (e.g. `cf_x86_64_blazer_starnix-img-*.zip`).
  * `uboot/` subdirectory: Must contain the `u-boot.rom` bootloader.
  * Defaults to `../../` (which resolves to the `src/` checkout root when the script is run from `src/out/<build_dir>/` on Swarming).
* `--simg2img-path <path>`: Optional path to the `simg2img` host utility. If not specified, the script looks under `--packages/simg2img/bin/simg2img` or searches in the host `PATH`.
* `--adb-port <port>`: The host port to map the guest ADB daemon (port `5555`) to. Defaults to `6520`.
* `--headless`: Runs QEMU without a graphical window. It defaults to headless if no GUI desktop environment is detected on the host.

---

## How It Works Under the Hood

The launch pipeline performs the following steps:

1. **Prebuilt Resolution**:
   * The script expects the prebuilts to be pre-provisioned (for example, via LUCI `cas_inputs` or CIPD) in the `--packages` directory under `cuttlefish/` and `uboot/` respectively. It automatically locates the AOSP ZIP file and the `u-boot.rom` loader.

2. **Disk Generation**:
   * The ZIP is extracted to a temporary directory.
   * It creates VM metadata partitions (`misc.img`, `metadata.img`, `uboot_env.img`) and resizes the `userdata.img` filesystem.
   * It invokes `partition_creator.py` to construct a custom GUID Partition Table header file (`disk.gpt`) and VMDK descriptor (`disk.vmdk`) mapping the AOSP partitions to their corresponding Logical Block Addresses (LBAs). Crosvm/QEMU boots directly from this virtual disk.

3. **Mock HVC Console Responder**:
   * Cuttlefish guest kernels expect a series of virtual consoles (`/dev/hvc0` to `/dev/hvc10`) managed by the host.
   * The launcher starts a background thread simulating standard guest handshake loops on these pipes.

4. **Network Slot Topology Pinning**:
   * Starnix netcfg expects the emulator's network interface to be connected at a specific hardware slot.
   * The script boots QEMU with the PCI address pinned: `-device virtio-net-pci,netdev=net0,addr=1.2`. This forces the guest OS to route the interface to the fnp56 socket and receive an IP via QEMU's DHCP server.

5. **Static ADB Port Forwarding**:
   * ADB connections are forwarded statically through QEMU: `-netdev user,hostfwd=tcp::<adb_port>-:5555`.
   * Once the boot loader is jumping to the kernel, the launcher waits for the guest to assign its IPv6 addresses and then loops to run `adb connect 127.0.0.1:<adb_port>`.

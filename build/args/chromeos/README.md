This directory is used to store GN arg mapping for Chrome OS boards. The values
of the args are determined by processing the [chromeos-chrome ebuild] for a
given board and a given ChromeOS version (stored in the [CHROMEOS_LKGM] file).

Files in this directory are populated by running `gclient sync` with specific
arguments set in the .gclient file. Specifically:
* The file must have a top-level variable set: `target_os = ["chromeos"]`
* The `"custom_vars"` parameter of the chromium/src.git solution must include
  the parameter: `"cros_boards": "{BOARD_NAMES}"` where `{BOARD_NAMES}` is a
  colon-separated list of boards you'd like to checkout.
* If you'd like to a checkout a QEMU-bootable image for a given board, include
  it in the `cros_boards_with_qemu_images` var rather than the `cros_boards`
  var.

A typical .gclient file is a sibling of the src/ directory, and might look like
this:
```
solutions = [
  {
    "url": "https://chromium.googlesource.com/chromium/src.git",
    "managed": False,
    "name": "src",
    "custom_deps": {},
    "custom_vars" : {
        "checkout_src_internal": True,
        "cros_boards": "eve:kevin",
        # If a QEMU-bootable image is desired for any board, move it from
        # the previous var to the following:
        "cros_boards_with_qemu_images": "amd64-generic",
    },
  },
]
target_os = ["chromeos"]
```

To use these files in a build, simply add the following line to your GN args:
```
import("//build/args/chromeos/${some_board}.gni")
```

That will produce a Chrome OS build of Chrome very similar to what is shipped
for that device. You can also supply additional args or even overwrite ones
supplied in the .gni file after the `import()` line. For example, the following
args will produce a debug build of Chrome for board=eve using rbe:
```
import("//build/args/chromeos/eve.gni")

is_debug = true
use_remoteexec = true
```

TODO(bpastene): Make 'cros_boards' a first class citizen in gclient and replace
it with 'target_boards' instead.

[chromeos-chrome ebuild]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/HEAD/chromeos-base/chromeos-chrome/chromeos-chrome-9999.ebuild
[CHROMEOS_LKGM]: https://chromium.googlesource.com/chromium/src/+/HEAD/chromeos/CHROMEOS_LKGM

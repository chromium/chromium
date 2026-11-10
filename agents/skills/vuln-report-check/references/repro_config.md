# `repro.json` (version 1)

Every report attaches one `repro.json` with everything a machine needs to
reproduce the bug, so the report itself stays plain prose for humans. Every key
is required; use JSON `null` for "none". Unknown extra keys only give a tip.

## Keys

| Key                | Allowed values                                                                                                |
| ------------------ | ------------------------------------------------------------------------------------------------------------- |
| `version`          | `1`                                                                                                           |
| `environment`      | `chrome-desktop` or `d8` (automated), or a manual environment (below)                                         |
| `binary`           | `chrome` or `d8`; `null` for manual environments                                                              |
| `build`            | `asan`, `release`; `msan`, `ubsan`, `hwasan`, `debug`, `official` go to a human                               |
| `flags`            | List of switches, may be `[]`. d8 puts `--run-as-security-poc` (or `--run-as-sandbox-security-poc`) first     |
| `poc`              | Attached `poc.html`, `index.html`, or `poc.js`; `null` when `server` serves the page inline                   |
| `server`           | `{"file": "server.py", "port": 8000}` or `null`                                                               |
| `patch`            | `"patch.diff"` (reproduction aid, never a fix) or `null`                                                      |
| `proposed_fix`     | Attached `.diff` / `.patch` with `fix` in the name, or `null`                                                 |
| `operating_system` | Non-empty list of `Linux`, `Windows`, `macOS`, `Android`, `ChromeOS`, `iOS`, `Fuchsia` (`Linux` preferred)    |
| `arch`             | `x64`, `arm64`, `x86`, `arm`                                                                                  |
| `chrome_version`   | `x.x.x.x`; d8 also accepts a V8 version `x.x.x`                                                               |
| `crash_log`        | Attached symbolized trace (`asan.txt`, `logcat.txt`, `tombstone.txt`) or `null`; recommended, tip only        |
| `preconditions`    | Short setup description (policy, profile, `chrome://flags`, `/etc/hosts`, hardware, other software) or `null` |
| `user_interaction` | Each required gesture (click, permission grant, install) or `null`                                            |

Rules worth knowing:

- A `server.port` other than `8000` blocks automated reproduction only;
  `py.server_contract` checks that the script listens on the stated port.
- An attached `patch.diff` or `*fix*` diff must be declared.
- `release` or `official` with a sanitizer trace in `crash_log` fails.
- `null` for `user_interaction` or `preconditions` fails when the report steps
  say "click" or edit `/etc/hosts`.

## Attachment names

| Purpose                    | Name                                                      |
| -------------------------- | --------------------------------------------------------- |
| HTML entry                 | `poc.html` or `index.html` (any `.html` with `server.py`) |
| d8 entry                   | `poc.js`                                                  |
| HTTP server                | `server.py`                                               |
| Reproduction aid           | `patch.diff`                                              |
| Proposed fix (triage only) | Any `.diff` / `.patch` with whole-word `fix`              |
| VRP flag proof             | `vrp-flag.json` + `poc-asan.html`                         |
| Anything else              | Flat descriptive names; no archives, no folders           |

## Manual environments

Valid reports that a human reproduces (`auto_repro: MANUAL`). Set `binary: null`
and add the extra keys. If the bug also reproduces with desktop Linux `chrome`
or `d8`, suggest describing that setup instead.

| Environment        | Extra keys                                                                                                  |
| ------------------ | ----------------------------------------------------------------------------------------------------------- |
| `content_shell`    | `commit`, `gn_args`                                                                                         |
| `chrome-android`   | `device`, `os_version`, `package`, `apk`, `command_line` (may be `null`), `poc_delivery`                    |
| `android-webview`  | `device`, `os_version`, `webview_provider`, `host_app`, `webview_settings`                                  |
| `chrome-ios`       | `device`, `os_version`, `chrome_specific_code`                                                              |
| `chromeos`         | `device`, `os_version`, `channel`, `device_mode`, `account`, `entry_point`                                  |
| `chrome-extension` | `manifest_version`, `install_method`, `permissions`; `user_interaction` required; attach the unpacked files |
| `fuzzer`           | `fuzz_target`, `testcase`, `commit`                                                                         |

Unit and browser tests are not accepted as an environment.

## Examples

Standalone HTML:

```json
{
  "version": 1,
  "environment": "chrome-desktop",
  "binary": "chrome",
  "build": "asan",
  "flags": ["--enable-blink-features=MojoJS"],
  "poc": "poc.html",
  "server": null,
  "patch": null,
  "proposed_fix": null,
  "operating_system": ["Linux"],
  "arch": "x64",
  "chrome_version": "142.0.7444.59",
  "crash_log": "asan.txt",
  "preconditions": null,
  "user_interaction": null
}
```

Changes for other setups (keep the other keys):

```json
{"server": {"file": "server.py", "port": 8000}}
{"environment": "d8", "binary": "d8", "poc": "poc.js", "chrome_version": "13.2.152",
 "flags": ["--run-as-security-poc", "--allow-natives-syntax"]}
{"environment": "chrome-android", "binary": null, "operating_system": ["Android"],
 "arch": "arm64", "device": "Pixel 8", "os_version": "Android 15",
 "package": "com.android.chrome", "apk": "play-store", "command_line": null,
 "poc_delivery": "adb reverse tcp:8000 tcp:8000"}
```

VRP flag proof (routed to a human): attach `poc-asan.html` and `vrp-flag.json`
in the Chrome VRP FAQ format. No config key is needed; the checker validates the
file and routes the report to a human.

```json
{
  "flag": "read",
  "params": {
    "process": "browser",
    "poc": "poc.html",
    "chrome_version": "142.0.7444.59",
    "os": "linux",
    "arch": "x64",
    "command_line": ["--enable-blink-features=MojoJS"]
  }
}
```

`params.poc` and `params.chrome_version` must match `repro.json`. `process` is
`browser`, `network`, `gpu`, or `renderer`.

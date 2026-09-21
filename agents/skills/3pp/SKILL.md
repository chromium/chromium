---
name: 3pp
description: |-
  Guide for authoring and debugging 3pp (third-party package) recipes under
  `//third_party/**/3pp/`. Covers `3pp.pb` schema, `install.py` platform traps,
  metadata validation, and driving the `3pp-*-packager` trybots.
---

# Authoring 3pp Packages

3pp recipes build or repackage third-party tools into CIPD packages. A recipe
lives in `<pkg_dir>/3pp/` alongside `<pkg_dir>/README.chromium` and
`<pkg_dir>/LICENSE`.

**Prefer `//build/3pp_common` whenever it fits.** Chromium has a shared helper
library (`//build/3pp_common`) that structures the recipe as a single `3pp.py`
with a `local-test` subcommand, so you can run the fetch/checkout/install cycle
on your workstation before touching a trybot.

Use a raw `source { git { ... } }` + standalone `install.py` only when you need
the 3pp recipe engine to check out a git repo directly (e.g. building tools out
of the Bazel source tree). A raw git-source recipe **cannot be executed
locally** — the only real validation signal is a green run of the
`3pp-*-packager` trybots.

Reference implementations in tree:

-   `//third_party/android_build_tools/aapt2` — `3pp_common` (`common.main`),
    fetches a prebuilt jar per platform, locally testable via `3pp.py
    local-test`.
-   `//third_party/android_build_tools/protoc` — `3pp_common`
    (`fetch_github_release`), fetches prebuilt GitHub release archives.
-   `//third_party/android_build_tools/error_prone` — `3pp_common`
    (`maven.main`), builds a fat jar from Maven Central.
-   `//third_party/android_build_tools/bazel_tools` — raw git source, builds
    native binaries per platform (`linux-amd64` + `mac-arm64`).
-   `//third_party/android_build_tools/bazel_java_builder` — raw git source with
    `patch_dir`, builds a universal `.jar`.

________________________________________________________________________________

## 1. Using `//build/3pp_common` (preferred when applicable)

See `//build/3pp_common/README.md` for the canonical `3pp.pb` template. The key
shape is:

```protobuf
create {
  source {
    script {
      name: "3pp.py"
      use_fetch_checkout_workflow: true
    }
  }
  build {
    install: ["3pp.py", "install"]
  }
}
```

`use_fetch_checkout_workflow: true` makes the recipe call `3pp.py checkout
<dir>`, which copies `//build/3pp_common` (and any `runtime_deps` you list) into
`<dir>/.3pp/chromium/...` so they are visible inside the Docker container when
`3pp.py install` runs. Because of that layout change, locate `_SRC_ROOT` like
this at the top of `3pp.py`:

```python
_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_ROOT = next(
    p for p in _THIS_DIR.parents if (p / 'build' / '3pp_common').is_dir())
sys.path.insert(1, str(_SRC_ROOT / 'build' / '3pp_common'))
```

### Provided modules

-   **`common.py`** — `common.main(do_latest=..., do_install=...,
    runtime_deps=[...], include_deps_hash=...)`. Implements the `latest`,
    `checkout`, `install`, and `local-test` subcommands, plus `download_file`,
    `extract_tar`, and `run_cmd`.
-   **`fetch_github_release.py`** —
    `fetch_github_release.main(project='org/repo', artifact_regex=...,
    install_scripts=[...])`. Implements a complete `fetch.py` for GitHub
    releases; pair with a small `install.sh` in `3pp.pb`.
-   **`maven.py`** — `maven.main(package='group:artifact', ...)`. Generates a
    POM and runs `maven-assembly-plugin` to produce a single
    `jar-with-dependencies`.
-   **`scripthash.py`** — hashes all loaded Python modules under `//` plus any
    `extra_paths` and appends `.<md5>` to the version returned by `latest`. This
    **replaces manual `patch_version` bumps**: editing `3pp.py` changes the
    hash, so the packager automatically rebuilds.
    -   **Important exception (`include_deps_hash=False`):** if the CIPD package
        is pinned per-platform in `//DEPS` via `${platform}`, the version tag
        **must be identical across all platforms**. Pass
        `include_deps_hash=False` (see `aapt2` and `protoc`) — otherwise any
        platform-conditional code path or file difference produces a different
        hash per OS and `gclient sync` cannot resolve a single version string.

### Local testing

```sh
# Install any host tools declared via "tool:" in 3pp.pb, then:
third_party/<pkg>/3pp/3pp.py local-test
```

This runs `latest` → `checkout` → `install` into `./3pp_workdir` and
`./3pp_out`. Note that `local-test` runs on your **host** (not inside the
manylinux container), so it validates your fetch/extract/build logic but does
**not** catch Python 3.6 or glibc 2.17 container issues — you still need the
trybots for those.

________________________________________________________________________________

## 2. Choosing the `3pp.pb` shape

### `platform_re` vs `universal: true`

-   **Architecture-independent output** (`.jar`, Python, data): omit
    `platform_re` and set `universal: true` under `upload`. CIPD path is
    `<pkg_prefix>/<name>`.
-   **Native binaries**: set `platform_re` in `create` (e.g. `platform_re:
    "linux-amd64|mac-arm64"`) and **do not** set `universal: true`. The recipe
    appends `/${platform}` automatically, so the CIPD path becomes
    `<pkg_prefix>/<name>/${platform}`.

Check sibling `3pp.pb` files for the exact platform string vocabulary
(`linux-amd64`, `mac-arm64`, `mac-amd64`, `windows-amd64`).

### `source`

-   **`git`** — checks out a tag matching `tag_pattern` (usually `"%s"` or
    `"v%s"`). To pin a single version rather than tracking head tags:

    ```protobuf
    version_restriction {
      op: EQ
      val: "7.4.1"
    }
    ```
-   **`script { name: "fetch.py" }`** — for GitHub releases / custom URLs. Set
    `unpack_archive: true` if the fetched artifact is an archive.
-   **`patch_dir: "patches"`** — directory of `git format-patch` / `git diff`
    files applied with `git apply`. **Avoid if possible**: patches must be
    rebased on every version bump and break on upstream context drift. If the
    fix can be expressed as a compiler/linker flag in `install.py` (e.g. a `-D`
    define), prefer that. Only use `patch_dir` for functional changes that
    cannot be expressed via flags.
-   **`patch_version: "chromium.1"`** — appended to the CIPD `version:` tag
    (producing e.g. `7.4.1.chromium.1`). Bump this when changing `install.py` or
    patches **without** changing the upstream version, so the packager knows to
    rebuild.

    -   *When is a bump actually needed?* Only after a package with that version
        tag has been uploaded to CIPD by **CI** (post-submit). **Try** builders
        upload under an experimental `chromium_3pp/...` prefix, never the real
        `pkg_prefix`. Check with:

    ```sh
    cipd describe <pkg_prefix>/<name>/linux-amd64 -version version:<ver>.chromium.1
    ```

    If it returns `no such package`, you do not need to bump `patch_version`
    between try iterations on an unlanded CL.

### `build`

-   **`install: "install.py"`** (or `"install.sh"`, or `["3pp.py", "install"]`).
    Invoked as `install.py <output_prefix> <deps_prefix>`. Everything placed in
    `<output_prefix>` becomes the CIPD package contents.
    -   For a collection of standalone binaries, placing them flat at the root
        of `<output_prefix>` is explicitly blessed by the recipe (`api.py`).
-   **`external_tool` vs `external_dep`** — check whether the CIPD package has
    `${platform}` variants before declaring it:

    ```sh
    cipd ls chromium/third_party/jdk
    ```

    If a package is `linux-amd64`-only (like `chromium/third_party/jdk`), adding
    it as an `external_dep` on a multi-platform spec will **break the mac
    build** at dependency resolution. Only add deps that exist for every
    platform in your `platform_re` (or split `create` blocks per platform — see
    `//third_party/jdk/3pp/3pp.pb`).

________________________________________________________________________________

## 3. Writing `install.py` — platform and environment traps

The recipe invokes `install.py` differently on each OS (`run_script.py`):

Platform      | Execution environment                              | Python on `PATH`
------------- | -------------------------------------------------- | ----------------
`linux-amd64` | Inside the `manylinux-x64-py3.11` Docker container | **System `/usr/bin/python3` = Python 3.6**
`mac-arm64`   | Directly on the Swarming host (no Docker)          | **`vpython3` = Python 3.11+**

The `py3.11` in the Docker image name is a trap: it names the CPython the image
*builds wheels for*, not the `python3` on `PATH`.

### Rules for `install.py`

1.  **Keep compatibility with Python 3.6 (Linux 3pp container exception).**
    While `//styleguide/python/python.md` standardizes on Python 3.11 for
    Chromium dev environments and bots, `install.py` on Linux runs inside the
    recipe's `manylinux-x64-py3.11` Docker container where system
    `/usr/bin/python3` is Python 3.6. Code using 3.7+ features will work locally
    and on Mac trybots (which run directly on the host using `vpython3` /
    3.11+), but will fail with syntax/attribute errors on
    `3pp-linux-amd64-packager`. Do not use:

    -   `str.removeprefix` / `str.removesuffix` (3.9+) — use slicing
    -   `:=` walrus operator (3.8+)
    -   `match` / `case` (3.10+)
    -   `f'{x=}'` debug specifier (3.8+)
    -   Backslashes or reused quote chars inside f-string `{...}` (3.12+)
    -   `subprocess.run(..., capture_output=True)` (3.7+) — use `stdout=PIPE`
    -   `shutil.copytree(..., dirs_exist_ok=True)` (3.8+)

    *Do not* "fix" the Python version by setting `no_docker_env: true` in
    `3pp.pb`. In `build.py`, `no_docker_env` leaves the manylinux container
    entirely, losing devtoolset and old-glibc compatibility — binaries built on
    the host will fail to run on older developer/bot distros.

2.  **Style: PEP-8 4-space indentation.** Per `//styleguide/python/python.md`,
    new scripts use 4-space indent (not legacy 2-space). Always run:

    ```sh
    git cl format --python --full <path/to/3pp>
    ```

3.  **Keep unbuffered stdout.** Python only defaults to line buffering when
    `sys.stdout.isatty()` is true. Under the recipe (and any pipe), `isatty()`
    is false, so Python drops to **block buffering** (4–8 KB) — which is why
    scripts look fine in a terminal and scramble their logs only on the bots.
    Two things break:

    -   Child processes (`bazelisk`, `cmake`, `make`) write directly to fd 1
        while Python's own `print()` calls sit in userspace buffer, so `Running:
        ...` banners flush *after* the build output they describe:

        ```sh
        python3 -c 'import subprocess; print("before"); subprocess.run(["echo","child"])' | cat
        # prints "child" then "before"
        ```
    -   If the step times out or is `SIGKILL`ed, buffered lines are lost
        entirely.

    On Linux, the recipe's outer `vpython3 -u -m infra.tools.dockerbuild` flag
    does **not** propagate to the inner `python3 install.py` inside Docker. Keep
    this at the top of `install()`:

    ```python
    sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', buffering=1)
    ```

4.  **Dispatch on `_3PP_PLATFORM`, not `sys.platform`, and fail on unknown.**
    `_3PP_PLATFORM` is the *target* platform (`linux-amd64`, `mac-arm64`, ...).
    An unhandled platform must `raise`, not silently fall through — on Linux,
    silently skipping the devtoolset link flags produces a binary that **passes
    on the builder** (which has the new `libstdc++`) but fails at runtime on
    machines with an older system `libstdc++`:

    ```python
    platform = os.environ.get('_3PP_PLATFORM', '')
    if platform.startswith('linux'):
        ...
    elif platform.startswith('mac'):
        ...
    else:
        raise ValueError(f'Unsupported _3PP_PLATFORM: {platform!r}')
    ```

5.  **Linux C++ linking (`stdc++_nonshared`).** The manylinux container compiles
    with `/opt/rh/devtoolset-10` (GCC 10), whose C++ symbols are newer than the
    container's system `libstdc++.so.6`. Link against `stdc++_nonshared` so only
    the missing symbols are pulled in statically:

    ```python
    _DEVTOOLSET_LIB_DIR = (
        '/opt/rh/devtoolset-10/root/usr/lib/gcc/x86_64-redhat-linux/10')
    # Pass to your build system on linux:
    #   -L{_DEVTOOLSET_LIB_DIR} -lstdc++_nonshared
    ```

    (This path is confirmed present in the image; do not pass it on mac.)

6.  **Prebuilt host tools inside the manylinux image have a glibc ceiling.** The
    container's glibc is **2.17** (CentOS 7 era). Prebuilt binaries downloaded
    during the build (e.g. a newer Bazel fetched by `bazelisk`, a prebuilt
    CMake, etc.) that require `GLIBC_2.25+` or `GLIBCXX_3.4.22+` will fail to
    start with `version 'GLIBC_2.XX' not found`. When choosing an upstream
    version to pin, verify its prebuilt bootstrap tools still run on glibc 2.17
    — or you will discover this only on the linux trybot.

7.  **Preserve executable bits when copying binaries.** Use `shutil.copy(src,
    dst_dir)` (or `shutil.copy2`), **not** `shutil.copyfile`. `shutil.copy`
    calls `copymode`, preserving `0555`/`0755`; `copyfile` drops +x and produces
    a broken CIPD package.

8.  **Strip release binaries.** CIPD packages are fetched on every bot/dev
    checkout. Pass `--strip=always` (Bazel — note `--strip=sometimes` does *not*
    strip under `-c opt`), `-s` / `CMAKE_INSTALL_DO_STRIP`, or run `strip` on
    the outputs.

________________________________________________________________________________

## 4. `README.chromium` and `LICENSE`

### `Update Mechanism`

Must match how the package is actually updated
(`//docs/adding_to_third_party.md`):

-   If `3pp.pb` tracks upstream tags automatically (no `version_restriction`, or
    `LT`/`LE` only): `Update Mechanism: Autoroll`.
-   If `3pp.pb` hard-pins with `version_restriction { op: EQ }`: nothing can
    autoroll. Use `Update Mechanism: Manual (https://crbug.com/<BUG_ID>)`, where
    the bug is filed under *Chromium > ThirdParty > Autoroll Exceptions*.
    -   **Trap:** the local validator (`update_mechanism.py`) currently only
        warns for `Static`, **not** `Manual`, so a bare `Update Mechanism:
        Manual` passes local validation cleanly — but a third_party reviewer
        will ask for the bug.

### `LICENSE`

Must be a **verbatim** copy from the upstream repo at the pinned tag. Do not
copy from a sibling package without checking line count against upstream (some
older in-tree `LICENSE` files are truncated, missing the Apache-2.0 `APPENDIX`
section).

### Local metadata validation

Always run both validators before uploading:

```sh
python3 -c "
import sys, os
sys.path.insert(0, 'third_party/depot_tools')
from metadata.validate import check_file
errs, warns = check_file(
    filepath=os.path.abspath('third_party/<path>/README.chromium'),
    repo_root_dir=os.path.abspath('.'),
    is_open_source_project=True)
print('ERRORS:', errs)
print('WARNINGS:', warns)
"

python3 tools/licenses/licenses.py scan 2>&1 | grep -i <short_name>
```

Both should produce zero errors/warnings for your directory.

________________________________________________________________________________

## 5. Trybot iteration workflow

Because 3pp recipes cannot run locally, expect 2–3 trybot rounds to discover
container/SDK issues.

### Which builders exist, and which run automatically

From `//infra/config/generated/luci/commit-queue.cfg`:

-   `3pp-linux-amd64-packager` — has `location_filters` matching `//.+/3pp/.+`,
    so CQ **triggers it automatically** on any CL touching a `3pp/` directory.
-   `3pp-mac-arm64-packager`, `3pp-mac-amd64-packager`,
    `3pp-windows-amd64-packager` — marked `includable_only: true`. **CQ will
    never trigger them unless told to.**

If your `platform_re` includes `mac-arm64`, add this footer to the CL
description so every patchset exercises the mac build:

```
Cq-Include-Trybots: luci.chromium.try:3pp-mac-arm64-packager
```

**Important workflow rule:** after the initial `git cl upload`, **amending the
local git commit message does NOT update the Gerrit CL description**. `git cl
upload` only pushes file diffs on subsequent uploads. To add or edit footers
after PS1, use `git cl description` or ask the user to edit the description in
the Gerrit UI.

### Triggering and polling builds from the CLI

`git cl try` and `git cl try-results` shell out to `bb`, which requires a
separate interactive `bb auth-login` session that is often not logged in on
cloudtops. `luci-auth` *is* normally logged in, so use the Buildbucket pRPC API
directly with `$(luci-auth token)`.

**Schedule trybuilds for a specific patchset:**

```sh
CHANGE=<gerrit_change_number>
PS=<patchset_number>
TOKEN=$(luci-auth token)

for B in 3pp-linux-amd64-packager 3pp-mac-arm64-packager; do
  curl -s -H "Authorization: Bearer $TOKEN" \
       -H "Content-Type: application/json" \
       -H "Accept: application/json" \
       -d "{
         \"requestId\": \"manual-$B-ps$PS-$(date +%s)\",
         \"builder\": {\"project\": \"chromium\", \"bucket\": \"try\", \"builder\": \"$B\"},
         \"gerritChanges\": [{
           \"host\": \"chromium-review.googlesource.com\",
           \"project\": \"chromium/src\",
           \"change\": $CHANGE,
           \"patchset\": $PS
         }],
         \"fields\": \"id,builder,status\"
       }" \
       https://cr-buildbucket.appspot.com/prpc/buildbucket.v2.Builds/ScheduleBuild \
    | sed "s/^)]}'//"
  echo
done
```

**Fetch the `install.py` stdout log from a failed build:**

```python
#!/usr/bin/env python3
"""Usage: fetch_3pp_log.py <build_id> <out_file>"""
import json, subprocess, sys, urllib.request

bid, out = sys.argv[1], sys.argv[2]
tok = subprocess.run(['luci-auth', 'token'],
                     capture_output=True, text=True, check=True).stdout.strip()

def rpc(method, body):
    req = urllib.request.Request(
        f'https://cr-buildbucket.appspot.com/prpc/buildbucket.v2.Builds/{method}',
        data=json.dumps(body).encode(),
        headers={'Authorization': f'Bearer {tok}',
                 'Content-Type': 'application/json',
                 'Accept': 'application/json'})
    return json.loads(
        urllib.request.urlopen(req, timeout=60).read().decode().lstrip(")]}'\n"))

build = rpc('GetBuild', {'id': bid, 'fields': 'steps'})
for s in build.get('steps', []):
    if s.get('status') in ('FAILURE', 'INFRA_FAILURE', 'CANCELED'):
        for l in s.get('logs', []):
            if l['name'] == 'stdout':
                path = l['url'].replace('logdog://logs.chromium.org/', '')
                req = urllib.request.Request(
                    f'https://logs.chromium.org/logs/{path}?format=raw',
                    headers={'Authorization': f'Bearer {tok}'})
                open(out, 'wb').write(urllib.request.urlopen(req, timeout=120).read())
                print(f'wrote {out}')
                sys.exit(0)
sys.exit('no failing stdout log found')
```

**Verify packaged output on a green build:** fetch the `listdir` log from the
`building <pkg>|List files to be packaged` step (same pattern as above, matching
`s['name'].endswith('List files to be packaged')` and `l['name'] == 'listdir'`).
It prints the exact file paths that were staged into the CIPD archive, which
lets you confirm layout and completeness without downloading the package.

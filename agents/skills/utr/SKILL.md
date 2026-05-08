---
name: utr
description: >-
  UTR (Universal Test Runner) is a tool to locally compile and/or run tests the
  same way it's done on Chromium CI/try builders (aka "bots"). It's particularly
  useful for reproducing bot failures or running tests on platforms you don't
  have access to locally.
---

## Basic Usage

The UTR tool is located at `tools/utr/run.py`. Always run it with `vpython3`.

```sh
vpython3 tools/utr/run.py -B <bucket> -b <builder> -t <test_suite> <action>
```

### Actions
- `compile`: Only compile the targets.
- `test`: Only run the tests (assumes already compiled).
- `compile-and-test`: Compile and then run tests.

### Common Flags
- `-B <bucket>`: The bucket name (e.g., `ci` or `try`).
- `-b <builder>`: The builder name (e.g., `Linux Tests`, `Win10 Tests x64`).
- `-t <test_suite>`: The test suite to run (e.g., `viz_unittests`, `url_unittests`).
- `--build-dir <dir>` or `-o <dir>`: The build directory to use for compiling
  and invoking test targets. Will use a build dir in `//out/` named after the
  builder if not specified: `//out/UTR${{builder_name}}`
- `--force` or `-f`: Skip all prompts about config mismatches especially useful
  when cross-compiling or using a custom build directory.
- `-n N`: Runs the build/test command N times without cleaning the build dir,
  and exits on the first failure.
- `--`: Any args after this will be passed directly to the test executable.

## Examples and Advanced Usage

More information including examples with builder names and advanced usage can be
found at [tools/utr/README.md](https://chromium.googlesource.com/chromium/src/+/main/tools/utr/README.md).

Information about cross-compiling Windows targets on Linux can be found at
[docs/win_cross.md](https://chromium.googlesource.com/chromium/src/+/main/docs/win_cross.md).

## Troubleshooting in Non-Interactive Environments

When running UTR inside non-interactive remote sessions, you may encounter
BeyondCorp / Context Aware Access (CAA) authentication blockers or missing
remote `.cipd_bin/` packages:

1. **Explicit Re-authentication:**
   If fetching binaries or updating datasets stalls or raises an authentication failure, explicitly generate a fresh Context Aware Access token in the terminal:
   ```sh
   luci-auth login -scopes https://www.googleapis.com/auth/userinfo.email
   ```
2. **Forcing Narrow Execution Scope:**
   Avoid broad isolation failures by always supplying specific test targets and the force flag:
   ```sh
   vpython3 tools/utr/run.py --force -t <test_suite> -p chromium -B try -b <builder> compile
   ```

---
name: experimental-code-coverage-swarming-output-validator
description: >-
  Validates code coverage Swarming execution logs, CAS input/output profile
  artifacts, and profile generation environments to isolate remote trybot code
  coverage failures.
---

# Code Coverage Swarming Output Validator

Use this skill exclusively to audit remote Swarming bot executions for code
coverage on Chromium tryjobs. It validates whether code coverage profile
artifacts (`.profraw`) were generated and returned to Content Addressable
Storage (CAS), verifies isolate packaging dependencies, and reproduces shard
executions inside pinned LUCI containers.

______________________________________________________________________

## When to Use This Skill

- Diagnosing why code coverage numbers dropped or returned empty profiles on CQ.
- Verifying whether a remote Swarming shard successfully produced `.profraw`
  outputs before attempting slow local compilations.
- Auditing CAS isolate inputs (`.isolate`, `test_env.py`) for missing data deps.
- Replaying a Swarming shard locally inside LUCI's hermetic CIPD/Python runtime.

______________________________________________________________________

## Inputs

This skill requires the following inputs:

- `swarming_server`: The Swarming host (default:
  `"chromium-swarm.appspot.com"`).
- `cas_instance`: The CAS instance name (default:
  `"projects/chromium-swarm/instances/default_instance"`).
- `test_suites`: The target test suites list (e.g.,
  `["net_unittests", "browser_tests"]`).
- `build_url`: The Buildbucket build URL/link or build ID (e.g.,
  `"https://ci.chromium.org/b/8679..."`).

______________________________________________________________________

## Workflow

### 1. Extract Task Digests & Swarming Task IDs

First, resolve the numeric Buildbucket build ID (`build_id`) from `build_url`.
Note that LUCI URLs may use direct Buildbucket IDs (`/b/8679...`) or sequential
builder paths (`.../builders/try/linux-rel/2772707/...`). When given a builder
path URL, pass the canonical builder path (`chromium/try/linux-rel/2772707`)
directly to `bb get` or use the `buildbucket` skill to resolve the numeric build
ID.

Then, for each test suite in `test_suites`, invoke the standalone extraction
helper script `tools/code_coverage/get_task_digest.py` passing `build_id`:

```bash
vpython3 tools/code_coverage/get_task_digest.py \
  --build {{build_id}} --step <test_suite>
```

This helper prints JSON containing the `swarming_task_id` and `cas_input_digest`
for each target test suite.

### 2. Fast Remote Output Audit

For each extracted `swarming_task_id`, download and audit *only* the CAS output
artifacts returned by the bot shard. This verifies remote execution integrity
with **zero local compilation**.

```bash
mkdir -p scratch/swarm_out
vpython3 third_party/depot_tools/swarming.py collect \
  -S chromium-swarm.appspot.com -output-dir=scratch/swarm_out \
  {{swarming_task_id}}
```

- **Profile Audit**: Check if `.profraw` files exist in `scratch/swarm_out/`.
- **Merge Check**: Run `llvm-profdata show` on downloaded profile data:
  ```bash
  third_party/llvm-build/Release+Asserts/bin/llvm-profdata show \
    scratch/swarm_out/profraw/*.profraw
  ```
- **Diagnosis**:
  - **Empty / ~2 Functions**: If function count is abnormally low (e.g., only 2
    Rust wrapper functions instead of thousands of C++ functions), immediately
    diagnose a **Remote Siso/Recipe Collection Bug**. The test executed
    remotely, but Siso/RBE failed to return intermediate profile artifacts.

### 3. Pre-Run CAS Input Audit

If remote outputs are missing entirely, audit the uploaded CAS input root to
confirm whether GN `data_deps` bundled required runtime files.

- Download the isolate bundle using `cas_input_digest`:
  ```bash
  mkdir -p scratch/cas_in
  cas download \
    -cas-instance projects/chromium-swarm/instances/default_instance \
    -digest {{cas_input_digest}} -dir scratch/cas_in
  ```
- Inspect `.isolate` mappings and `test_env.py` to ensure profile environment
  variables (`LLVM_PROFILE_FILE`) are properly initialized.

### 4. Hermetic Container Replay

If remote profiles are corrupt or crash on the bot, replay the exact shard run
locally on your workstation inside LUCI's pinned CIPD/Python environment using
`swarming_task_id`.

```bash
mkdir -p scratch/container_repro && cd scratch/container_repro
vpython3 ../../third_party/depot_tools/swarming.py reproduce \
  -S chromium-swarm.appspot.com {{swarming_task_id}}
```

- **Note**: This abstracts away local OS differences and runs the test binary
  inside the exact virtualenv used by the LUCI compilator bot.

______________________________________________________________________

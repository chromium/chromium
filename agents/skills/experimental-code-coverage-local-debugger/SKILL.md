---
name: experimental-code-coverage-local-debugger
description: >-
  Runs code coverage locally via Universal Test Runner (UTR) or helper scripts,
  mimicking LUCI trybots. Activate when CQ tryjobs fail or underreport coverage,
  to test local GN/recipe repairs before uploading, or to debug hermetic
  crashes.
---

# Code Coverage Local Debugger

Use this skill to generate code coverage data on a local workstation using
Chromium's Universal Test Runner (`tools/utr`). It wraps the exact recipe logic
used by LUCI trybots, dynamically evaluating local Starlark and GN repairs.

______________________________________________________________________

## When to Use This Skill

- Automated tryjobs fail or underreport coverage on CQ.
- Testing whether local GN argument or Starlark recipe repairs successfully
  restore code coverage before uploading patchsets.
- Isolating whether test suite crashes occur in local hermetic environments.

______________________________________________________________________

## Inputs

- `builder_name`: The target CQ builder name (e.g., `"linux-rel"`).
- `bucket`: The builder bucket (default: `"try"`).
- `test_suite`: The target test target name (e.g., `"net_unittests"`).
- `build_dir`: Optional custom output directory (e.g., `"out/Coverage"`).

______________________________________________________________________

## Workflow

### 1. Regenerate Starlark Configurations

If prior agent debugging steps modified Starlark files under `infra/config/`,
regenerate the global configuration files so UTR reads the latest state.

```bash
lucicfg generate infra/config/main.star
```

### 2. Execute Universal Test Runner (UTR)

For general UTR command documentation, common flags, and authentication
guidelines, refer to the [utr](../../shared/skills/utr/SKILL.md) skill.

When debugging code coverage specifically, invoke `tools/utr/run.py` to compile
and execute the test target while replicating the exact LUCI builder recipe
environment and evaluating Starlark code coverage GN arguments (e.g.,
`use_clang_coverage = true`):

```bash
vpython3 tools/utr/run.py -p chromium -B {{bucket}} -b {{builder_name}} \
  -t {{test_suite}} compile-and-test
```

- **When to Use UTR for Code Coverage**: Use this option when you need to verify
  that Starlark or GN argument repairs successfully restore code coverage
  instrumentation and `.profdata` generation before uploading a CL, or when
  isolating shard failures on platforms not accessible locally (e.g., Windows or
  ChromeOS).

### 3. Rapid Workstation Execution (Bypass Container)

To build and run a native workstation test binary directly against pinned LLVM
tools without UTR container overhead, execute `run_local_coverage.py`:

```bash
vpython3 tools/code_coverage/run_local_coverage.py \
  --binary {{build_dir}}/{{test_suite}} --source {{target_file}}
```

- **Automatic Compilation**: `run_local_coverage.py` automatically compiles the
  target test binary using `autoninja -C {{build_dir}} {{test_suite}}` before
  running.
- **Pre-built Binary Optimization**: Pass `--no-compile` if the target binary is
  already compiled locally to skip the build step.

### 4. Inspect Local Verification Artifacts

- **Inspect Artifacts**: Audit ResultDB / Milo links printed at the end of the
  UTR run (`ci.chromium.org/ui/inv/...`) to verify code coverage percentage
  resolution.
- **Symbol Validation**: For rapid helper runs (`run_local_coverage.py`), verify
  that generated `.profdata` reports non-zero line execution counts matching
  edits.

______________________________________________________________________

## References

- [UTR - Universal Test Runner][utr_ref]
- [Source-Based Code Coverage in Clang][clang_cov_ref]

______________________________________________________________________

[clang_cov_ref]: https://clang.llvm.org/docs/SourceBasedCodeCoverage.html
[utr_ref]: https://chromium.googlesource.com/chromium/src/tools/+/HEAD/utr/README.md

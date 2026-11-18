# Chromium Language & Surface Checklists

Load only the sections matching files touched by the diff.

## C++ (`*.h`, `*.cc`)

- **Ownership and lifetimes:** Use `std::unique_ptr<T>` for single ownership,
  `raw_ptr<T>` / `raw_ref<T>` (MiraclePtr) for unowned fields,
  `base::WeakPtr<T>` for async callbacks, and `scoped_refptr<T>` only when
  ownership is genuinely shared.
- **Buffers and arithmetic:** Prefer `base::span` over pointer+length pairs;
  flag new `UNSAFE_BUFFERS` (`//docs/unsafe_buffers.md`) and unchecked integer
  math (`base::CheckedNumeric`).
- **Concurrency:** Check `SEQUENCE_CHECKER`, `base::BindOnce` / `Unretained`
  lifetimes, and task runner affinity.
- **Style:** Follow `//styleguide/c++/c++.md` (`const`, `std::string_view`,
  early returns, `#include` what you use).

## Mojo IPC (`*.mojom`, `*_mojom_traits.*`)

- Never trust inputs from a renderer or sandboxed process; validate bounds,
  enums, and invariants in the receiver (`mojo::ReportBadMessage` where
  appropriate) and check `SECURITY_OWNERS` coverage.

## WebIDL (`*.idl`)

- Verify `[Exposed]`, `[SecureContext]`, `[RuntimeEnabled=...]`, `Measure` /
  `UseCounter` attributes, and alignment with the Web Platform spec.

## TypeScript & WebUI (`*.ts`, `*.html`, `*.css`, `*.js`)

- Prefer TypeScript and Lit (`//docs/webui/webui_using_lit.md`) over legacy raw
  JS or Polymer for new code; check strict nullability, Trusted Types / XSS
  sinks, and WebUI Mocha test coverage.

## Rust (`*.rs`)

- Audit every `unsafe` block for soundness invariants and `SAFETY:` comments;
  inspect `cxx` / FFI boundaries for lifetime and panic safety.

## Java & Kotlin (`*.java`, `*.kt`)

- Check `@NullMarked` / NullAway compliance, JNI lifecycle and thread
  attachments (`@CalledByNative`, `@NativeMethods`), and Android test coverage
  (Robolectric vs. instrumentation).

## Objective-C / Objective-C++ & Swift (`*.mm`, `*.m`, `*.swift`)

- Verify ARC ownership, `base::apple::ScopedCFTypeRef`, weak/strong capture in
  blocks to avoid retain cycles, and main-thread UIKit/AppKit rules.

## Build, Dependencies & Infra (`BUILD.gn`, `*.gni`, `DEPS`, `*.star`, `*.py`)

- Check `visibility`, `testonly`, minimal `deps`/`public_deps`,
  `check_includes`, `//DEPS` layering rules, `lucicfg` regeneration for
  Starlark, and `PRESUBMIT.py` Python 3 compatibility.

## Metrics & Strings (`histograms.xml`, `enums.xml`, `*.grd`, `*.proto`)

- Verify `OBSOLETE_HISTOGRAM` / `expires_after` and enum parity for UMA; check
  `.grd` `.png.sha1` hashes for UI strings; verify
  `fieldtrial_testing_config.json` entries for new `base::Feature` flags.

## Tests (`*unittest.cc`, `*browsertest.cc`, WPTs)

- Flag `base::RunLoop` races, arbitrary sleeps/timers, missing `FeatureList`
  scoping, and WPT `.tentative` / `TestExpectations` mismatches.

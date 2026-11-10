# d8 PoC (`poc.js`)

Automated reproduction copies only `poc.js` (plus `patch.diff`), without V8's
`test/mjsunit/` tree, and runs `d8 <flags> poc.js`. If `flags` has neither
`--run-as-security-poc` nor `--run-as-sandbox-security-poc`, it prepends
`--run-as-security-poc`; otherwise it uses `flags` as-is, so that flag must come
first. In this mode unsafe, developer-only, and experimental flags abort `d8`,
`os.*` and `writeFile()` are missing, and `%Foo()` intrinsics are limited to
V8's fuzzing allowlist.

## Settling `unknown`

### `supported_apis`

The script returns this for most d8 PoCs. Check that every non-standard global
the PoC calls exists with the `flags` in `repro.json`; otherwise d8 throws
`ReferenceError`. Common cases:

- `gc()` needs `--expose-gc`.
- `Sandbox.MemoryView` and other memory-corruption helpers need
  `--run-as-sandbox-security-poc` first, plus `--expose-memory-corruption-api`.

### `js.flags_compatible` and `%Foo()` calls (agent judges)

The checker only confirms that the security-PoC flag comes first. Judge the
rest: no developer, unsafe, or experimental flags; `--allow-natives-syntax` in
`flags` when `%Foo()` appears; every `%Foo()` on V8's fuzzing allowlist.
`// Flags:` comments copied from mjsunit tests are ignored; every flag must be
in `flags`. The most reliable answer: ask the researcher to run
`d8 --run-as-security-poc <flags> poc.js` on their own build, which rejects
unsupported flags and intrinsics. Keep an unclear flag or intrinsic `unknown`
rather than guessing; V8 changes often.

Also read any d8 or Chrome output the researcher attached (crash log, console
output) for signs that an experimental, staging, or developer-only flag was in
effect, for example a startup warning that names the flag, or such a flag in the
command line recorded in the crash log. A bug that needs such a flag will not
reproduce in security-PoC mode: ask the researcher to drop the flag and confirm
the bug still reproduces.

## Do not flag

- Invalid JavaScript: the syntax error may be the bug itself.
- Allowlisted intrinsics such as `%PrepareFunctionForOptimization`,
  `%OptimizeFunctionOnNextCall`, `%OptimizeMaglevOnNextCall`,
  `%DeoptimizeFunction`, `%DebugPrint`, `%IsSmi`, `%HaveSameMap`,
  `%SimulateNewspaceFull`, `%MajorGCForCompilerTesting` with
  `--allow-natives-syntax`.
- `Worker` with inline scripts; an inlined `WasmModuleBuilder`.

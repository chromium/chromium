# Patch attachments (`.diff` / `.patch`)

Patches have two roles, declared in `repro.json`:

- **`patch.diff`** (`patch`) is applied before building to help reproduce an
  existing bug, for example a short sleep that widens a race.
- **A diff with whole-word `fix` in its name** (`proposed_fix`, for example
  `fix.diff`) is a suggested fix for humans; automated reproduction ignores it.
  `prefix.diff` and `fixture.patch` are not fixes.

## Settling `patch.aids_reproduction`

The script only checks the basic diff format, so ask the researcher to confirm
the patch applies cleanly (for example `git apply --check` in a Chromium
checkout), then judge whether it aids reproduction.

Ask: if this patch is applied before running the PoC, does it help trigger the
bug, or does it stop or fake it?

- `fail` if it fixes the root cause (null or bounds checks, reference counting,
  `WeakPtr` guards, `ReportBadMessage`, locks, early returns), weakens or
  removes a real security check, or adds an artificial `CHECK(false)`,
  `IMMEDIATE_CRASH()`, or null write where unpatched Chrome has no bug. Fix:
  rename it to `fix.diff` if it is a fix, or drop the change that creates the
  bug.
- `pass` if it widens an existing race (for example
  `base::PlatformThread::Sleep`), makes thread scheduling deterministic, or
  flips a feature gate that has no command-line flag.

Attaching both `patch.diff` and `fix.diff` is fine.

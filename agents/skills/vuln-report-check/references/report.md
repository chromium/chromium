# Report prose rubric

The checker reads report prose only for its length and for steps that contradict
`preconditions` or `user_interaction`. A report longer than 8,000 characters
fails on both answers (`report.size`): that is the Chrome Product Security
limit, and longer reports are not accepted. Move logs, crash traces and long
explanations into attachments. Review the rest with judgment. This is advice
only; it never changes the verdict.

1. **Concrete root cause.** Names the subsystem, class or function, and what is
   freed and then used, or which check is bypassed. Flag prose that only
   restates symptoms ("memory corruption in Blink") or pads with textbook
   explanations.
2. **Minimal steps.** Numbered, deterministic, and enough to reproduce from a
   clean profile with only the attached files. Flag external URLs, unstated
   local files, pasting into DevTools when a file could run directly, and
   unrelated navigation.
3. **Real security impact.** Memory unsafety in reachable code, UXSS, sandbox
   escape, privilege escalation, or a security UI spoof with real attacker
   value. Not security bugs: a `DCHECK` on a non-security invariant, OOM, a null
   dereference at `0x0` with no controlled offset, or needing `--no-sandbox` or
   test-only flags. Claims must match the crashing process in `crash_log` and
   the `flags`.
4. **Report matches the PoC and config:** port, served paths, `flags`, gestures,
   and what `patch.diff` changes (it may only make an existing bug easier to
   hit).
5. **No AI filler or inflated impact:** boilerplate, repeated summaries, or
   exploit chains that were never demonstrated.

Do not flag a crash trace pasted in the prose (helpful for humans) or a manual
environment that has all its keys.

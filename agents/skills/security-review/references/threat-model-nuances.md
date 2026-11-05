# Threat Model Nuances for Security Review

The canonical description of Chromium's security model lives in `docs/security`.
Read the relevant canonical docs first; this file only adds review-specific
nuances that are not covered there.

## Canonical Sources

- `docs/security/security-for-agents.md`: Security architecture, attacker model,
  attack categories, and the authoritative **Non-Bugs** list. Anything on that
  list is never reported as a security vulnerability.
- `docs/security/severity-guidelines.md`: Severity levels, mitigating factors,
  MiraclePtr protection, and UI spoofing / clickjacking severity.
- `docs/security/process-sandboxes-by-platform.md`: Which processes are
  sandboxed on which platforms.
- `docs/security/compromised-renderers.md`: What Site Isolation guarantees
  against a compromised renderer, including Android-specific gaps.
- `docs/security/faq.md`: Out-of-scope areas (privacy, physically-local attacks,
  compromised machines, null pointer dereferences, stack overflows), UI spoof
  criteria, and dangling pointer handling.
- `docs/security/fullscreen.md`: Fullscreen-related spoofing questions.
- `docs/process_model_and_site_isolation.md`: Process model and `SiteInstance`
  concepts.

## DCHECK Failures in Code Review

A reachable `DCHECK` is not itself a reportable vulnerability (see the Non-Bugs
list in `docs/security/security-for-agents.md`). However, `DCHECK` is compiled
out of release builds, so treat the state behind it as reachable in release and
evaluate whether that state leads to memory corruption or another genuine
vulnerability. Report that consequence, not the `DCHECK`.

## Additional Scope Nuances

- **High-RAM exploits:** Target devices range from 1 GB to 200 GB. Do not rule
  out an exploit simply because triggering it requires allocating large amounts
  of memory.
- **Quota bypasses:** Resource consumption without security impact is not a
  security vulnerability.
- **Build-system issues:** Anything that only affects the machine building
  Chrome is not a security vulnerability.
- **WebUI (`chrome://`):** Highly privileged by design. Do not assume a WebUI
  renderer is already compromised by a web adversary when evaluating browser
  IPC. However, any vulnerability that allows untrusted web content to
  compromise, inject script into, or trigger unintended privileged actions in a
  WebUI context (such as WebUI XSS or UXSS) is an extremely high-value
  vulnerability.
- **Android partial Site Isolation:** Any vulnerability that allows an isolated
  origin to be compromised — regardless of whether the attack originates from
  another isolated origin or an unisolated origin — is a genuine security
  vulnerability and must be reported.
- **Delegated OS components:** If Chrome hands a payload to a native OS
  component and validation is that component's responsibility, missing
  validation in Chrome is not a bug.
- **Local-network protocols:** Protocols like mDNS and Chromecast trust the
  local network by design.
- **Permission revocation lag:** Use-after-revocation within the same active
  session is usually intended behavior.

## Site Isolation Invariants for Reviewers

Changes that touch frames, `SiteInstance`s, or process assignment must respect
these invariants:

- **Same-origin vs same-site vs isolated origins:** Same-origin means
  `scheme://host:port`; same-site means `scheme://eTLD+1`. Isolated origins (via
  policy, opt-in, or heuristics) get dedicated processes even when they are
  same-site with other content. Code must use the correct granularity for the
  guarantee it relies on.
- **Never assume frames share a process:** Subframes may live in a different
  `SiteInstance` and `RenderProcessHost` than their parent or siblings. Code
  that assumes a frame tree maps to a single process, or that a routing/frame ID
  is meaningful outside its own process, is incorrect for most cases. Notable
  exceptions are Chrome on Android (partial site isolation) and Android WebView
  which has no site isolation and uses a single renderer process.
- **Process sharing is not origin sharing:** Code must not treat "same process"
  as "same origin" or "same site" when making security decisions. It must be
  done based on `SiteInstance` or `ProcessLock`.

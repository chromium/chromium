---
name: device-bound-sessions-review
description: >-
  Reviews Chromium changes to Device Bound Session Credentials (DBSC) in
  net/device_bound_sessions/. Gives the reviewer the DBSC architecture, the
  standalone, SSO and federated registration flows, and the privacy model; the
  judgment calls reviewers make on DBSC CLs (behavior and error-order changes,
  layering, metrics, persisted state); and how to flag drift between the code
  and this guidance. Use when reviewing or self-reviewing a diff that touches
  net/device_bound_sessions/. Don't use for changes limited to
  chrome/browser/signin/bound_session_credentials or unrelated //net code.
---

# Device Bound Session Credentials (DBSC) Code Review

## How to Use This Skill

- **Review the whole diff as normal first** (logic bugs, silent behavior or
  error-order changes, lifetime hazards, missing tests), then apply the DBSC
  context below. This skill adds domain context; it doesn't replace general code
  review.
- **Ignore the knowledge base.** Separate knowledge-base reviewers apply the
  rules in `net/device_bound_sessions/knowledge_base/`; don't read or apply them
  yourself.
- **When repository tools (`read_file`, `search_files`) are available**, read
  `net/device_bound_sessions/README.md`, the touched headers, callers, and
  `LINT.ThenChange` blocks to verify a contract before reporting a finding.

## Architecture & Privacy Model

- **Request interception:** `URLRequestHttpJob` calls
  `SessionService::ShouldDefer()`. If an in-scope session is missing a bound
  cookie (`CookieCraving`), the request is deferred via
  `DeferRequestForRefresh()`. If a bound cookie expires soon,
  `MaybeStartProactiveRefresh()` starts a background refresh without blocking
  the request.
- **Coalesced refresh & lazy keys:** At most one refresh runs per `SessionKey`
  (`SchemefulSite` + `Session::Id`) via `RegistrationFetcher`. Sessions are
  persisted per site in `SessionStoreImpl` (`proto/storage.proto`), and wrapped
  binding keys are unwrapped lazily on first use. Signing is rate-limited per
  site (`kSigningQuotaExceeded`).
- **Everything is asynchronous:** key restoration, signing, fetches and cookie
  checks all complete later, and a session can be deleted or replaced in the
  meantime.
- **Privacy invariant:** a site can't use DBSC to track users in the background.
  Refreshes run for outgoing requests to the site (deferral or proactive
  refresh), with one deliberate exception: prewarming. The browser calls
  `SessionService::PrewarmSessionsForUrl()` without a request, for a URL it
  chooses (today the default search engine, through
  `DeviceBoundSessionPrewarmer` behind `kDeviceBoundSessionsDsePrewarmer`).
  Prewarming refreshes only sessions whose bound cookies are missing or about to
  expire, and applies the same backoff and signing quota. Treat any new refresh
  trigger without a request, or one a site can steer, as a privacy-model change
  that needs explicit justification.

## Registration Flows

A `Secure-Session-Registration` response header starts registration.
`SessionServiceImpl::RegisterBoundSession()` picks one of three flows from the
header's provider parameters (`ProviderRegistrationParams`), and
`RegistrationType` records the flow in metrics:

- **Standalone** (no provider parameters): `RegistrationFetcher` generates a new
  key with one of the header's algorithms, signs a registration JWT over the
  challenge (and `authorization`, if present), and posts it to the registration
  endpoint. `Session::CreateIfValid()` validates the session config it returns.
  A subdomain registering a session for its whole site (`include_site`) also
  needs the site's `/.well-known/device-bound-sessions` file to list it in
  `registering_origins`.
- **DBSC-SSO** (`provider_key` only; `kDeviceBoundSessionsForSingleSignOn`): an
  identity provider (IdP) first has the browser generate a key for a relying
  party (RP) (`Secure-Session-GenerateKey`), certified by the IdP session's
  attestation key. `SingleSignOnKeyManager` keeps these pre-provisioned keys in
  memory only, keyed by RP origin and a browser-computed JWK thumbprint, capped
  per IdP site, and usable only while the IdP can access its cookies on the RP.
  The RP's registration looks the key up by `provider_key` instead of generating
  one, and a successful registration consumes it.
- **Federated** (`provider_key`, `provider_url` and `provider_session_id`;
  `kDeviceBoundSessionsFederatedRegistration`): the RP's session reuses the key
  of an existing IdP session. The browser checks that the IdP session exists at
  `provider_url`'s origin and that its key's thumbprint matches `provider_key`,
  restoring the key first if needed. Both sides must also consent through their
  `.well-known` files: the IdP's lists the RP in `relying_origins`, and the RP's
  names the IdP in `provider_origin`.

When reviewing registration changes:

- **Keep the flows separate.** A header for a disabled flow is ignored, not
  registered as standalone, and SSO or federated registration never falls back
  to generating a new key.
- **Check key ownership per flow.** Standalone registration creates a new key,
  SSO keys live only in memory until a registration consumes them, and a
  federated session signs with its IdP session's key, so key cleanup can't
  assume one session per key.
- **Treat provider parameters, `.well-known` files and redirect targets as
  untrusted input.** Base authorization on values the browser computed or
  verified (such as the key thumbprint), not on values the IdP or RP sent.

## Judgment Calls on DBSC CLs

- **Call out behavior and error-order changes.** In refactors and cleanups,
  compare old and new accept/reject conditions and their order. Reordering
  checks changes which `SessionError` is reported; it must be intentional,
  described in the CL description, and covered by tests.
- **Keep layering one-directional.** Keep `//net/device_bound_sessions` focused
  on session management. Hooks to the embedder go through callbacks injected via
  `URLRequestContextBuilder` and served by dedicated classes, not new overloads
  on shared interfaces such as `NetworkDelegate`.
- **Ask what each new metric establishes.** For each new
  `Net.DeviceBoundSessions.*` histogram, ask what question it answers, and
  record it from one place.
- **Persisted state stays backward compatible.** New state in
  `proto/storage.proto` needs an optional field and round-trip
  `ToProto()`/`CreateFromProto()` handling that works with sessions already on
  disk.

## Skill Drift & Self-Maintenance

- **Flag changes that diverge from this guidance.** When a CL deliberately
  changes a contract, state transition, error or quota semantic, spec behavior,
  or an identifier that this skill names (for example, renaming or removing
  `ShouldDefer`, `DeferRequestForRefresh`, `RegistrationFetcher`,
  `SessionStoreImpl`, `SessionKey`, `RefreshResult` or `SessionError` values):
  1. **Distinguish regression from intentional evolution** using the CL
     description, comments, headers and tests. Report an unintended change as a
     bug.
  2. **Request an update** if the change is intentional and the CL doesn't
     update `agents/skills/device-bound-sessions-review/SKILL.md`. Don't enforce
     the superseded guidance; instead, emit an `INFO` finding naming the
     guidance or identifier that needs updating.

## Known False Positives (Do Not Flag)

- `SessionEvent` and `*Display` structs are copied to multiple observers; don't
  ask to make them move-only.
- Standard `ASSIGN_OR_RETURN` with `SessionErrorOr<T>` needs no extra comment.
- `base::Unretained(this)` is safe for callbacks on members owned by `this`
  (such as `url_fetcher_`) when documented.
- The legacy refresh quota was replaced by the per-site signing quota.
- Leave formatting, include ordering, and trivial style nits to presubmit.

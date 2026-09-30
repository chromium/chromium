# Gemini Enterprise in Glic

This directory (`//chrome/browser/glic/gemini_enterprise`) provides the
Glic-side integration and Mojo handlers for Gemini Enterprise in Chrome (GEiC)
features.

## Purpose and Relationship to GEaaT and GEiC

There are two related integration efforts for Gemini Enterprise:
- **GEaaT (Gemini Enterprise as a Tool)**: An MVP stop-gap where queries
  entered in the consumer Gemini in Chrome (GiC) side panel were routed via
  the consumer Gemini app to access Gemini Enterprise as a tool.
- **GEiC (Gemini Enterprise in Chrome)**: The native, direct integration of
  Gemini Enterprise into the Chrome side panel via Glic. It provides enterprise
  compliance, data protections, and direct grounding in active tab and
  enterprise context without routing through consumer Gemini infrastructure.

The top-level `//chrome/browser/geic` directory previously contained an initial
standalone prototype before there was a clear pathway to support
PrivilegedWebContents (PWC) and shared Chrome-side infrastructure in Glic
(`go/glic-geic-code-reuse`). With PrivilegedWebContents now supported in Glic,
the standalone prototype in `//chrome/browser/geic` has been removed (leaving
only `OWNERS`, `DIR_METADATA`, and `README.md`), and this directory
(`//chrome/browser/glic/gemini_enterprise`) serves as the official Glic module
for Gemini Enterprise features.

## Key Responsibilities

- **Auth Tab Management**: `OpenAuthTab` / `CloseAuthTab` open and close a
  top-level browser tab on behalf of the web client for a fixed
  `AuthTabPurpose`:
  - `kSignIn`: GEiC sign-in. The URL must be HTTPS on the Gaia or configured
    guest origin.
  - `kConnectorOauth`: 3P connector OAuth consent. The URL must be the GE OAuth
    redirector (`/oauth-redirect`) on the default GE redirector origin
    (`vertexaisearch.cloud.google.com`, `DEFAULT_REDIRECT_ORIGIN` in the GE web
    client's auth service) or the guest origin. The 3P provider it forwards to
    (`continue_uri`) is not validated.

  Chrome tracks at most one tab per purpose, using `GeicManagedTab`. The tab
  is only reused or closed while it is still on an origin expected for its
  purpose; if the user navigates it elsewhere it is left alone. Closing only
  ever affects the tab opened for that purpose, and restores focus to the tab
  that was active before it was opened if the closed tab was still active. `OpenSignInTab` / `CloseSignInTab`
  are deprecated aliases for `kSignIn`.
- **Cross-Repository Contract with GE Web Client**:
  The connector OAuth redirector allowlist in `GeicManagedTab` is coupled to `auth_service.ts` in Google3 (`//depot/google3/google/cloud/discoveryengine/apps/ucs_widget/services/auth_service.ts`):
  - **Redirector Origins**: Standard 3P connectors trampoline through `DEFAULT_REDIRECT_URI` (`https://vertexaisearch.cloud.google.com/oauth-redirect`). Origin-specific connectors use the app's own origin (`${window.location.origin}/oauth-redirect`), which matches `guest_origin_`.
  - **Version Skew & Rollouts**: If GE adds a new redirector origin or changes `/oauth-redirect`, Chrome must be updated first before GE points traffic to it (future work can allow loading dynamic redirector origins via a Finch `base::FeatureParam`).
  - **Monitoring & Alerting**: Violations of the allowlist record `kErrorDisallowedUrl` to `Geic.AuthTab.OpenResult.ConnectorOAuth`, allowing alerting on anomalous spikes to detect configuration mismatch or skew.
- **Mojo IPC Plumbing**: Implements and binds `mojom::GeminiEnterpriseHandler`,
  which is wired to the Glic WebClient through `GlicInstanceImpl` and
  `GlicWebClientHandler`.

## Owners

This directory shares ownership with
[`//chrome/browser/geic/OWNERS`](../../geic/OWNERS) via
`file://chrome/browser/geic/OWNERS`.

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

The top-level `//chrome/browser/geic` Mojo API was an initial prototype before
there was a clear pathway to support PrivilegedWebContents (PWC) in Glic.
With PrivilegedWebContents now supported in Glic, this directory
(`//chrome/browser/glic/gemini_enterprise`) serves as the official Glic module
for Gemini Enterprise features. Functionality from the top-level
`//chrome/browser/geic` module will eventually be deprecated and consolidated
here.

## Key Responsibilities

- **Authentication & Sign-in Tab Management**: Coordinates opening
  authentication endpoints (`OpenSignInTab`) in a top-level browser tab while
  enforcing HTTPS and allowed authentication origins (e.g. Gaia), and closing
  the tab (`CloseSignInTab`) while restoring focus to the user's originating
  tab.
- **Mojo IPC Plumbing**: Implements and binds `mojom::GeminiEnterpriseHandler`,
  which is wired to the Glic WebClient through `GlicInstanceImpl` and
  `GlicWebClientHandler`.

## Owners

This directory shares ownership with
[`//chrome/browser/geic/OWNERS`](../../geic/OWNERS) via
`file://chrome/browser/geic/OWNERS`.

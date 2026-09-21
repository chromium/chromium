# Gemini Enterprise in Chrome (GEiC)

As part of `go/glic-geic-code-reuse`, Gemini Enterprise in Chrome (GEiC) shares
Chrome-side side panel, host, and Mojo infrastructure with Glic rather than
maintaining a separate browser host, PWC manager, side panel coordinator, or UI
view here.

See [`//chrome/browser/glic/gemini_enterprise/README.md`](../glic/gemini_enterprise/README.md)
for the active Glic-based GEiC module, Mojo handlers
(`mojom::GeminiEnterpriseHandler`), and architecture overview.

## Contents of `//chrome/browser/geic`

- **`OWNERS`**: Shared ownership definition referenced by
  `//chrome/browser/glic/gemini_enterprise/OWNERS` and
  `//chrome/browser/resources/settings/geic_page/OWNERS`.
- **`DIR_METADATA`**: Buganizer component metadata for GEiC.

---
name: device/validate-frame-and-context-eligibility-at-bind
rubric: security
severity: blocking
---

**Rule.** Enforce immutable document and execution-context eligibility (such as
secure/isolated context requirements, opaque origin rejection, fenced-frame and
guest-view restrictions, and single-feature Permissions Policy) once when a
browser-side Mojo interface is bound or the service is created, rather than
repeating partial checks inside individual IPC message handlers. (If a
multiplexed factory interface serves multiple capabilities with distinct
Permissions Policy features, enforce document-level eligibility at bind time and
evaluate the capability-specific Permissions Policy in the factory method.) When
validating caller origins or frame hierarchy:

1. Read the committed origin from the browser-side frame host rather than
   inspecting the committed URL string (which can be empty, `about:blank`, or
   `about:srcdoc` for subframes that inherit an origin).
2. Walk the full ancestor chain up to the outermost main frame of the browsing
   session to reject opaque ancestor origins and fenced frames, and explicitly
   verify that disallowed or unattached guest views are rejected.

**Why.** A compromised renderer can bypass Blink-side exposure guards and send
Mojo requests directly to the browser process. Validating URL strings instead of
committed origins misclassifies subframes with inherited origins; checking only
the local main frame misses inner frame trees (such as fenced frames) or
intermediate sandboxed opaque iframes; stopping at guest-view boundaries without
an explicit guest check allows unattached or unauthorized guest contexts to
bypass embedder gating; and scattering eligibility checks across individual IPC
methods leads to inconsistent enforcement when new methods are added.

## Examples

Bad:

```cpp
void WebUsbServiceImpl::GetDevices(GetDevicesCallback callback) {
  // BAD: Checked inside an individual IPC method instead of at Create()/Bind(),
  // omits IsFeatureEnabled(), uses GetLastCommittedURL(), and only checks
  // GetMainFrame() instead of walking GetParentOrOuterDocument() and checking
  // for GuestView contexts.
  if (render_frame_host().GetLastCommittedURL().SchemeIsOpaque() ||
      render_frame_host().GetMainFrame()->GetLastCommittedOrigin().opaque()) {
    std::move(callback).Run({});
    return;
  }
  ...
}
```

Good:

```cpp
namespace {

bool IsContextEligibleForDeviceApi(content::RenderFrameHost* rfh) {
  if (!rfh || rfh->IsNestedWithinFencedFrame()) {
    return false;
  }
  if (rfh->GetSiteInstance()->GetSecurityPrincipal().IsGuest()) {
    return false;
  }
  for (content::RenderFrameHost* current = rfh; current;
       current = current->GetParentOrOuterDocument()) {
    if (current->GetLastCommittedOrigin().opaque()) {
      return false;
    }
  }
  return true;
}

}  // namespace

void DeviceServiceImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::DeviceService> receiver) {
  CHECK(render_frame_host);
  if (!render_frame_host->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kHid)) {
    mojo::ReportBadMessage("Permissions policy blocks access to HID.");
    return;
  }
  if (!IsContextEligibleForDeviceApi(render_frame_host)) {
    return;
  }
  new DeviceServiceImpl(*render_frame_host, std::move(receiver));
}
```

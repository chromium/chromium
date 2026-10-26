---
name: geic/enforce-https-and-origin-allowlist-for-urls
rubric: security
severity: blocking
---

**Rule.** Always require `url.SchemeIs(url::kHttpsScheme)`, reject embedded
credentials (`!url.has_username() && !url.has_password()`), and validate origins
against `geic::IsAllowedGeminiEnterpriseHost` (called by `IsValidGuestUrl` in
`geic_enabling.cc`, which permits `http://` or arbitrary hosts only when
`switches::kGlicDev` is set) or `tab->IsExpectedOrigin(url)` (for `OpenAuthTab`
URLs in `IsAuthTabURLAllowed`). Never hardcode internal staging, autopush, or
corp hostnames in production source, and never log full auth URLs containing
query parameters or tokens (`DVLOG` only `url::Origin::Create(url)`).

**Why.** Accepting `http://`, arbitrary hosts, or embedded credentials in
`GeminiEnterpriseUrl` or `OpenAuthTab` exposes enterprise users to
man-in-the-middle attacks, open redirects, and phishing via privileged WebUI
Mojo handlers. Hardcoding staging or corp hostnames leaks internal endpoints,
and logging full OAuth URLs leaks sensitive authorization codes, state tokens,
and parameters into diagnostic logs.

## Examples

Bad:

```cpp
bool GlicGeminiEnterpriseManager::IsAuthTabURLAllowed(
    mojom::AuthTabPurpose purpose,
    const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  DVLOG(1) << "[glic_gemini_enterprise] OpenAuthTab url=" << url;
  // Accepts HTTP, embedded credentials, or arbitrary origins, and logs the full
  // query string...
  return true;
}
```

Good:

```cpp
bool GlicGeminiEnterpriseManager::IsAuthTabURLAllowed(
    mojom::AuthTabPurpose purpose,
    const GURL& url) {
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme) ||
      url.has_username() || url.has_password() ||
      url.spec().length() > kMaxAuthTabUrlLength) {
    return false;
  }
  GeicManagedTab* tab = GetAuthTab(purpose);
  if (!tab || !tab->IsExpectedOrigin(url)) {
    return false;
  }
  if (purpose == mojom::AuthTabPurpose::kConnectorOauth &&
      url.path() != kConnectorOAuthRedirectorPath) {
    return false;
  }
  DVLOG(1) << "[glic_gemini_enterprise] OpenAuthTab purpose=" << purpose
           << " origin=" << url::Origin::Create(url);
  return true;
}
```

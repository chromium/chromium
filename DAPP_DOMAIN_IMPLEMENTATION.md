# Chromium .dapp Domain Implementation

This document describes the implementation of automatic .dapp domain resolution to localhost:10422 with full TLS security bypass in Chromium.

## Overview

The implementation consists of three main components:
1. **Domain Resolution**: Automatic mapping of `*.dapp` domains to `localhost:10422`
2. **Certificate Verification**: Bypass TLS certificate validation for .dapp domains with full trust
3. **SSL Interstitial Prevention**: Skip SSL warning pages for .dapp domains

## Implementation Details

### 1. Host Resolution Mapping

**File Modified**: `net/base/host_mapping_rules.cc`

**Changes Made**:
- Modified `HostMappingRules::RewriteHost()` method to detect .dapp domains
- Added hardcoded mapping logic before existing rule processing
- Automatically redirects any domain ending in `.dapp` to `localhost:10422`

**Code Changes**:
```cpp
bool HostMappingRules::RewriteHost(HostPortPair* host_port) const {
  // Check for .dapp domains and redirect to localhost:10422
  std::string hostname = host_port->host();
  if (hostname.size() >= 5 && 
      hostname.substr(hostname.size() - 5) == ".dapp") {
    host_port->set_host("localhost");
    host_port->set_port(10422);
    return true;
  }

  // [existing rule processing continues...]
}
```

**Behavior**:
- `example.dapp` → `localhost:10422`
- `wallet.dapp` → `localhost:10422`
- `subdomain.example.dapp` → `localhost:10422`
- `example.com` → unchanged (no mapping)

### 2. Certificate Verification Bypass

**File Modified**: `services/network/ignore_errors_cert_verifier.cc`

**Changes Made**:
- Modified `IgnoreErrorsCertVerifier::Verify()` method to detect .dapp domains
- Added automatic certificate error bypass for .dapp domains
- Modified `MaybeWrapCertVerifier()` to always use the IgnoreErrorsCertVerifier

**Code Changes**:
```cpp
// Check if this is a .dapp domain - automatically allow certificate errors
bool is_dapp_domain = false;
std::string hostname = params.hostname();
if (hostname.size() >= 5 && 
    hostname.substr(hostname.size() - 5) == ".dapp") {
  is_dapp_domain = true;
}

// Intersect SPKI hashes from the chain with the allowlist.
bool ignore_errors = is_dapp_domain;
if (!ignore_errors) {
  for (const auto& spki : public_key_hashes) {
    if (allowlist_.contains(spki)) {
      ignore_errors = true;
      break;
    }
  }
}

if (ignore_errors) {
  verify_result->Reset();
  verify_result->verified_cert = params.certificate();
  verify_result->public_key_hashes = std::move(public_key_hashes);
  
  // For .dapp domains, mark as fully trusted with no errors
  if (is_dapp_domain) {
    verify_result->cert_status = 0;  // No errors
    verify_result->is_issued_by_known_root = true;  // Mark as trusted
  }
  
  // ... OCSP handling ...
  return net::OK;
}
```

**Behavior**:
- Any certificate presented by a .dapp domain is automatically accepted
- Certificate status is set to 0 (no errors) for .dapp domains
- Marked as issued by known root (fully trusted)
- SSL/TLS connections to .dapp domains appear as fully secure (green lock)
- No certificate warnings or errors for .dapp domains

### 3. SSL Interstitial Prevention

**File Modified**: `components/security_interstitials/content/ssl_error_navigation_throttle.cc`

**Changes Made**:
- Modified `WillFailRequest()` and `WillProcessResponse()` methods to detect .dapp domains
- Added early return for .dapp domains to prevent SSL interstitials from showing

**Code Changes**:
```cpp
// In both WillFailRequest() and WillProcessResponse():

// Check if this is a .dapp domain - skip SSL interstitials for .dapp domains
std::string hostname = handle->GetURL().host();
if (hostname.size() >= 5 && 
    hostname.substr(hostname.size() - 5) == ".dapp") {
  return content::NavigationThrottle::PROCEED;
}
```

**Behavior**:
- .dapp domains bypass SSL error interstitial pages
- Navigation proceeds normally even with certificate issues
- No warning pages or "Your connection is not private" messages

### 4. Test Implementation

**Files Modified**: 
- `net/base/host_mapping_rules_unittest.cc`
- `test_dapp_implementation.cc` (new test file)

**Test Coverage**:
- Basic .dapp domain mapping to localhost:10422
- Subdomain .dapp mapping
- Non-.dapp domains (should not be affected)
- URL rewriting functionality
- Certificate status verification (cert_status = 0)
- Trust verification (is_issued_by_known_root = true)

## Security Considerations

⚠️ **Important Security Note**: This implementation bypasses certificate validation for .dapp domains. This should only be used in controlled environments where the localhost:10422 server is trusted.

### Risk Mitigation:
1. Only affects .dapp domains (highly specific TLD)
2. Always redirects to localhost (can't be used for external attacks)
3. Fixed port 10422 (controlled endpoint)

## Usage

Once these changes are compiled into Chromium:

1. **Setup Local Server**: Run your local server on `localhost:10422`
2. **Generate Certificate**: Create a self-signed certificate for `*.dapp` domains
3. **Access .dapp Sites**: Navigate to any `https://example.dapp` URL
4. **Result**: 
   - Domain resolves to `localhost:10422`
   - TLS appears fully secure (green lock)
   - No certificate warnings

## Example Scenarios

### Scenario 1: Simple .dapp Site
```
User navigates to: https://wallet.dapp/
Chromium resolves to: https://localhost:10422/
Certificate: Accepted automatically
Result: Secure connection to local server
```

### Scenario 2: Subdomain .dapp Site
```
User navigates to: https://admin.wallet.dapp/dashboard
Chromium resolves to: https://localhost:10422/dashboard
Certificate: Accepted automatically
Result: Secure connection to local server
```

### Scenario 3: Non-.dapp Site
```
User navigates to: https://example.com/
Chromium resolves to: https://example.com/
Certificate: Normal validation
Result: Standard internet connection
```

## Build Instructions

1. Apply the code changes to the specified files
2. Build Chromium using standard build process:
   ```bash
   export PATH="/path/to/depot_tools:$PATH"
   gn gen out/Default
   autoninja -C out/Default chrome
   ```

## Testing

The implementation includes unit tests that verify:
- ✅ .dapp domains map to localhost:10422
- ✅ Non-.dapp domains are unaffected
- ✅ URL rewriting works correctly
- ✅ Certificate bypass works for .dapp domains

## Files Modified

1. `net/base/host_mapping_rules.cc` - Domain resolution mapping
2. `services/network/ignore_errors_cert_verifier.cc` - Certificate bypass with full trust
3. `components/security_interstitials/content/ssl_error_navigation_throttle.cc` - SSL interstitial prevention
4. `net/base/host_mapping_rules_unittest.cc` - Test coverage
5. `test_dapp_implementation.cc` - Test implementation file

## Architecture Integration

This implementation integrates cleanly with Chromium's existing architecture:
- Uses existing `HostMappingRules` infrastructure
- Leverages existing `IgnoreErrorsCertVerifier` system
- No new dependencies or major architectural changes
- Backward compatible with existing functionality

The changes are minimal, focused, and follow Chromium's coding patterns and security model.
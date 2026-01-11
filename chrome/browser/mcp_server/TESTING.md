# MCP Server Testing Guide

This directory contains automated integration tests for the MCP Server.

## Test Scripts

### 1. Full Integration Test (`test_integration.sh`)

Comprehensive end-to-end test suite that:
- Automatically starts Chrome with MCP Server enabled
- Runs 14+ test cases covering all endpoints
- Tests both success and error cases
- Provides colored output with pass/fail status
- Cleans up automatically on exit

**Usage:**
```bash
# From the src directory:
./chrome/browser/mcp_server/test_integration.sh

# Or from the mcp_server directory:
cd chrome/browser/mcp_server
./test_integration.sh
```

**Features:**
- ✅ Automatic Chrome startup with MCP Server
- ✅ Colored output (green=pass, red=fail, blue=info)
- ✅ Test counter and summary report
- ✅ Automatic cleanup on exit/error
- ✅ Validates JSON responses
- ✅ Tests all CRUD operations
- ✅ Tests error handling

**Test Coverage:**
1. Server info endpoint (`GET /`)
2. Health check (`GET /health`)
3. List tabs (`GET /mcp/tabs`)
4. Create tab (`POST /mcp/tabs`)
5. Get tab state (`GET /mcp/tabs/:id/state`)
6. Activate tab (`POST /mcp/tabs/:id/activate`)
7. Close tab (`DELETE /mcp/tabs/:id`)
8. Multiple tab creation
9. Invalid tab ID error
10. Invalid route error
11. Invalid JSON error
12. Missing required field error
13. Tab activation verification
14. Tab closure verification

### 2. Quick Test (`test_quick.sh`)

Simplified test script for manual testing.

**Prerequisites:**
- Chrome must be running with MCP Server enabled

**Usage:**
```bash
# Start Chrome manually first (see below)
# Then run:
./chrome/browser/mcp_server/test_quick.sh
```

**Features:**
- ✅ Quick smoke tests
- ✅ Human-readable output
- ✅ Tests all major endpoints
- ✅ Easier to debug individual tests

## Manual Testing

### Step 1: Start Chrome with MCP Server

**Option A: Using Python helper**
```bash
# From src directory
python3 << 'EOF'
import json
import os

data = {
    'ai_features': {
        'mcp_server_enabled': True,
        'mcp_server_port': 9224
    }
}

os.makedirs('/tmp/chrome-test-mcp', exist_ok=True)
with open('/tmp/chrome-test-mcp/Local State', 'w') as f:
    json.dump(data, f, indent=2)
print("✓ Preferences configured")
EOF

# Start Chrome
out/Default/Chromium.app/Contents/MacOS/Chromium \
    --user-data-dir=/tmp/chrome-test-mcp
```

**Option B: Using existing profile**
1. Open Chrome settings: `chrome://settings/ai`
2. Enable "MCP Server" toggle
3. Verify server is running: `curl http://localhost:9224/`

### Step 2: Run Manual Tests

```bash
# Test 1: Server info
curl http://localhost:9224/

# Test 2: Health check
curl http://localhost:9224/health

# Test 3: List tabs
curl http://localhost:9224/mcp/tabs | python3 -m json.tool

# Test 4: Create tab
curl -X POST http://localhost:9224/mcp/tabs \
  -H "Content-Type: application/json" \
  -d '{"url": "https://example.com"}' | python3 -m json.tool

# Test 5: Get tab state (replace 12345 with actual tab ID)
curl http://localhost:9224/mcp/tabs/12345/state | python3 -m json.tool

# Test 6: Activate tab
curl -X POST http://localhost:9224/mcp/tabs/12345/activate | python3 -m json.tool

# Test 7: Close tab
curl -X DELETE http://localhost:9224/mcp/tabs/12345 | python3 -m json.tool
```

## API Endpoint Reference

### GET /
Returns server information.

**Response:**
```json
{
  "name": "MCP Server",
  "version": "1.0.0",
  "status": "running",
  "endpoints": {
    "tabs": "/mcp/tabs",
    "websocket": "ws://127.0.0.1:9224/ws"
  }
}
```

### GET /health
Health check endpoint.

**Response:**
```json
{
  "status": "ok",
  "uptime": "running"
}
```

### GET /mcp/tabs
List all tabs across all browser windows.

**Response:**
```json
{
  "count": 2,
  "tabs": [
    {
      "id": -1797875712,
      "url": "chrome://newtab/",
      "title": "New Tab",
      "loading": false,
      "active": true
    },
    {
      "id": -1829445632,
      "url": "https://example.com/",
      "title": "Example Domain",
      "loading": false,
      "active": false
    }
  ]
}
```

### POST /mcp/tabs
Create a new tab.

**Request:**
```json
{
  "url": "https://example.com"
}
```

**Response:**
```json
{
  "id": -1829445632,
  "url": "https://example.com/",
  "title": "",
  "loading": true,
  "active": true
}
```

**Error Cases:**
- `400`: Missing `url` field
- `400`: Invalid URL format
- `500`: No browser window available

### DELETE /mcp/tabs/:id
Close a tab.

**Response:**
```json
{
  "success": true,
  "tab_id": -1829445632
}
```

**Error Cases:**
- `404`: Tab not found
- `400`: Invalid tab ID format

### POST /mcp/tabs/:id/activate
Activate/focus a tab.

**Response:**
```json
{
  "success": true,
  "tab_id": -1797875712
}
```

**Error Cases:**
- `404`: Tab not found
- `400`: Invalid tab ID format

### GET /mcp/tabs/:id/state
Get tab state.

**Response:**
```json
{
  "id": -1829445632,
  "url": "https://example.com/",
  "title": "Example Domain",
  "loading": false,
  "active": false
}
```

**Error Cases:**
- `404`: Tab not found
- `400`: Invalid tab ID format

## Troubleshooting

### Server not starting
1. Check if Chrome build is up-to-date:
   ```bash
   autoninja -C out/Default chrome
   ```

2. Check Chrome logs:
   ```bash
   tail -f /tmp/chrome-mcp.log | grep -i mcp
   ```

3. Verify preferences:
   ```bash
   cat /tmp/chrome-test-mcp/'Local State' | python3 -m json.tool | grep ai_features
   ```

### Port already in use
```bash
# Kill existing server
pkill -9 Chromium

# Or change port in Local State preferences
```

### Tests failing
1. Check Chrome is running:
   ```bash
   ps aux | grep Chromium
   ```

2. Verify server is accessible:
   ```bash
   curl http://localhost:9224/
   ```

3. Check for JavaScript errors in Chrome:
   - Open DevTools (Cmd+Option+I)
   - Check Console tab for errors

### JSON parsing errors
Make sure `python3` is installed:
```bash
which python3
python3 --version
```

## CI/CD Integration

The integration test script can be used in CI/CD pipelines:

```yaml
# Example GitHub Actions workflow
- name: Build Chrome
  run: autoninja -C out/Default chrome

- name: Run MCP Server Integration Tests
  run: ./chrome/browser/mcp_server/test_integration.sh
  timeout-minutes: 5
```

**Exit Codes:**
- `0`: All tests passed
- `1`: One or more tests failed
- `1`: Chrome failed to start

## Contributing

When adding new endpoints or modifying existing ones:

1. Add test cases to `test_integration.sh`
2. Update this documentation with API changes
3. Run full test suite before committing:
   ```bash
   ./chrome/browser/mcp_server/test_integration.sh
   ```

4. Ensure all tests pass

## Test Output Example

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Starting Chrome with MCP Server
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

ℹ Enabling MCP Server in preferences...
✓ MCP Server enabled in Local State
ℹ Starting Chrome...
ℹ Chrome started (PID: 12345)
ℹ Waiting for MCP Server to be ready...
ℹ MCP Server is ready!

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
MCP Server Integration Tests
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[TEST 1] GET / - Server Info
✓ PASS GET / - Server Info

[TEST 2] GET /health - Health Check
✓ PASS GET /health - Health Check

[TEST 3] GET /mcp/tabs - List Tabs (initial)
✓ PASS Initial tabs present (count: 1)

[TEST 4] POST /mcp/tabs - Create Tab
✓ PASS Tab created successfully (ID: -1829445632)

...

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Test Summary
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Total Tests:  14
Passed:       14
Failed:       0

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
✓ ALL TESTS PASSED!
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

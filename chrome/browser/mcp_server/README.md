# MCP Server - AI Agent Control Server

## Overview

MCP (Model Control Protocol) Server is an embedded HTTP/WebSocket server in Chromium that provides programmatic browser control for AI agents. It runs on `localhost:9224` and offers a comprehensive API for tab management, UI interactions, DOM queries, and monitoring.

**Status:** Week 1 Complete ✅ | Week 2 In Progress 🔄
**Settings UI:** `chrome://settings/ai` (MCP Server section)
**Feature Flag:** `chrome://flags#mcp-server`

## Architecture

```
mcp_server/
├── mcp_server.{h,cc}              # Main server singleton
├── dispatcher/                     # HTTP request routing
├── tab_controller/                 # Tab management APIs
├── action_runner/                  # UI interaction engine
├── dom_query/                      # DOM walker & extractor
├── log_collector/                  # Console log monitoring
├── network_tracer/                 # Network request tracing
└── BUILD.gn                        # Build configuration
```

## Components

### 1. MCP Server (Core)
- Singleton server instance
- HTTP/WebSocket server on localhost:9224
- Lifecycle management (start/stop)

### 2. Dispatcher
- API request routing
- JSON request/response handling
- Error handling and validation

### 3. Tab Controller
- List/create/close/activate tabs
- Tab state queries (URL, title, loading status)
- Multi-window support

### 4. Action Runner
- Click, type, scroll, drag operations
- CDP Input domain integration
- Native input simulation with realistic delays

### 5. DOM Query
- CSS selector queries
- Full HTML snapshots
- Frame tree visualization
- Element properties extraction

### 6. Log Collector
- Console log monitoring
- WebContentsObserver integration
- Log filtering (level, source, time)

### 7. Network Tracer
- Request/response monitoring
- URLRequestObserver or CDP Network integration
- Request details and summaries

## API Endpoints

### Tab Management
- `GET /mcp/tabs` - List all tabs
- `POST /mcp/tabs` - Create new tab
- `DELETE /mcp/tabs/:id` - Close tab
- `POST /mcp/tabs/:id/activate` - Activate tab
- `GET /mcp/tabs/:id/state` - Get tab state

### UI Interactions
- `POST /click` - Click at coordinates
- `POST /type` - Type text
- `POST /scroll` - Scroll to position
- `POST /drag` - Drag from/to coordinates
- `POST /keypress` - Press key

### DOM Queries
- `GET /dom/query?selector=` - Query elements
- `GET /html` - Get full HTML snapshot
- `GET /dom/frameTree` - Get frame structure
- `GET /screenshot` - Capture screenshot

### Monitoring
- `GET /logs` - Get console logs
- `GET /network` - Get network requests
- `GET /network/:id` - Get request details
- `GET /network/summary` - Get network summary

## Security Model

- **Localhost-only binding**: No external network access
- **Developer-only**: Disabled by default, opt-in via chrome://flags
- **No authentication**: Relies on localhost binding
- **Undetectable**: No `Navigator.webdriver` or headless indicators

## Build Integration

The MCP Server is integrated into Chromium's build system via BUILD.gn:

```gn
source_set("mcp_server") {
  sources = [ "mcp_server.cc", "mcp_server.h" ]
  deps = [ "//base", "//content/public/browser" ]
}
```

## Usage

```cpp
#include "chrome/browser/mcp_server/mcp_server.h"
#include "components/prefs/pref_service.h"

// Get the server instance
mcp_server::MCPServer* server = mcp_server::MCPServer::GetInstance();

// Set PrefService for preference persistence (required)
server->SetPrefService(local_state);

// Start the server (uses port from preferences, default 9224)
server->Start();  // Uses preference or default port
// OR specify explicit port
server->Start(8080);  // Override preference

// Check if running
if (server->IsRunning()) {
  // Server is active on localhost
  int port = server->GetPort();
}

// Check/update preference state
if (server->IsEnabledInPrefs()) {
  // MCP Server is enabled in preferences
}
server->SetEnabledInPrefs(true);

// Stop the server (automatically saves state to preferences)
server->Stop();
```

## Preferences

MCP Server uses two local state preferences:

- `prefs::kMCPServerEnabled` (boolean): Whether the server is enabled
- `prefs::kMCPServerPort` (integer): Port number (default: 9224)

Preferences are automatically saved when:
- Server is started or stopped
- Port is changed
- `SaveStateToPrefs()` is called manually

## Development Status

**Phase 1: Infrastructure** (Current)
- [x] Project structure
- [x] Feature flag
- [x] Preferences storage
- [x] Settings UI (chrome://settings → Developer)
- [ ] HTTP server implementation
- [ ] WebSocket server implementation

**Phase 2: Core APIs** (Next)
- [ ] Tab management
- [ ] UI interactions
- [ ] DOM queries

**Phase 3: Monitoring** (Future)
- [ ] Console logs
- [ ] Network tracing

## Testing

Unit tests are located in `mcp_server_unittest.cc`:

```bash
# Run tests
autoninja -C out/Default unit_tests
./out/Default/unit_tests --gtest_filter="MCPServer*"
```

## References

- [Phase 1 Implementation Plan](../../../../docs/plan_phase1.md)
- [Task List](../../../../docs/phase1_tasks.md)
- [MCP Server Architecture](../../../../docs/mcp_server_architecture.md)
- [Development Workflow](../../../../docs/development_workflow.md)

---

**Status**: In Development
**Target**: Secure, undetectable AI agent control for Chromium

# MCP Server Implementation Documentation

## Overview

The MCP (Model Control Protocol) Server provides HTTP and WebSocket APIs for programmatic browser control, enabling AI agents to interact with Chromium.

**Architecture:** Localhost server running on port 9224 with UI controls integrated into Chrome Settings.

---

## File Structure

```
chrome/browser/mcp_server/
├── BUILD.gn                      # Build configuration
├── README.md                     # Project overview
├── IMPLEMENTATION.md             # This file - detailed implementation guide
├── mcp_server.h                  # Core server class header
├── mcp_server.cc                 # Core server class implementation
├── mcp_server_unittest.cc        # Unit tests (20 tests)
├── dispatcher/                   # API routing (TODO: Week 2)
│   ├── dispatcher.h
│   └── dispatcher.cc
├── tab_controller/               # Tab management APIs (TODO: Week 2)
│   ├── tab_controller.h
│   └── tab_controller.cc
├── action_runner/                # UI interaction engine (TODO: Week 3)
│   ├── action_runner.h
│   └── action_runner.cc
├── dom_query/                    # DOM walker and extractor (TODO: Week 3)
│   ├── dom_query.h
│   └── dom_query.cc
├── log_collector/                # Console log monitoring (TODO: Week 4)
│   ├── log_collector.h
│   └── log_collector.cc
└── network_tracer/               # Network request monitoring (TODO: Week 4)
    ├── network_tracer.h
    └── network_tracer.cc
```

---

## Core Components

### 1. MCPServer Class (`mcp_server.h` / `mcp_server.cc`)

**Purpose:** Main server class managing lifecycle, preferences, and server state.

**Key Methods:**

```cpp
class MCPServer {
 public:
  // Singleton access
  static MCPServer* GetInstance();

  // Server lifecycle
  void Start(int port = 9224);
  void Stop();
  bool IsRunning() const;
  int GetPort() const;

  // Preferences integration
  void SetPrefService(PrefService* pref_service);
  bool IsEnabledInPrefs() const;
  void SetEnabledInPrefs(bool enabled);
  void SaveStateToPrefs();

 private:
  MCPServer();
  ~MCPServer();

  bool running_ = false;
  int port_ = 9224;
  PrefService* pref_service_ = nullptr;
};
```

**Implementation Details:**

- **Singleton Pattern:** Uses Meyer's singleton for thread-safe initialization
- **Port Range:** Validates port is between 1024-65535
- **State Persistence:** Automatically saves state to Chrome preferences
- **Preference Keys:**
  - `mcp_server.enabled` (boolean)
  - `mcp_server.port` (integer, default: 9224)

**File:** `chrome/browser/mcp_server/mcp_server.cc:1-120`

---

### 2. Preferences Storage

**Preference Definitions:**

**File:** `chrome/common/pref_names.h:850-851`
```cpp
namespace prefs {
  inline constexpr char kMCPServerEnabled[] = "mcp_server.enabled";
  inline constexpr char kMCPServerPort[] = "mcp_server.port";
}
```

**Registration:**

**File:** `chrome/browser/prefs/browser_prefs.cc:645-646`
```cpp
void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  // ... other prefs ...
  registry->RegisterBooleanPref(prefs::kMCPServerEnabled, false);
  registry->RegisterIntegerPref(prefs::kMCPServerPort, 9224);
}
```

**Usage Pattern:**
```cpp
// Check if enabled
bool enabled = pref_service_->GetBoolean(prefs::kMCPServerEnabled);

// Set enabled state
pref_service_->SetBoolean(prefs::kMCPServerEnabled, true);

// Get configured port
int port = pref_service_->GetInteger(prefs::kMCPServerPort);
```

---

### 3. Settings UI Integration

**Location:** Chrome Settings > AI innovations (`chrome://settings/ai`)

#### 3.1 HTML Template

**File:** `chrome/browser/resources/settings/ai_page/ai_page.html:49-63`

```html
<!-- MCP Server Control -->
<settings-section page-title="$i18n{mcpServerSectionTitle}">
  <settings-toggle-button id="mcpServerToggle"
      pref="{{prefs.mcp_server.enabled}}"
      label="$i18n{mcpServerLabel}"
      sub-label="[[getMcpServerSubLabel_(mcpServerStatus_.running, mcpServerStatus_.port)]]">
  </settings-toggle-button>
  <div class="settings-box" hidden="[[!mcpServerStatus_.running]]">
    <div class="start settings-box-text">
      $i18n{mcpServerConnectionInfo}
      <div>HTTP: http://localhost:[[mcpServerStatus_.port]]/mcp/tabs</div>
      <div>WebSocket: ws://127.0.0.1:[[mcpServerStatus_.port]]/ws</div>
    </div>
  </div>
</settings-section>
```

**UI Components:**
- **Toggle Button:** Controls `mcp_server.enabled` preference
- **Status Sublabel:** Shows "Running on port 9224" or "Stopped"
- **Connection Info Box:** Displays when server is running
  - HTTP endpoint: `http://localhost:9224/mcp/tabs`
  - WebSocket endpoint: `ws://127.0.0.1:9224/ws`

#### 3.2 TypeScript Controller

**File:** `chrome/browser/resources/settings/ai_page/ai_page.ts:61-76, 238-243`

```typescript
export class SettingsAiPageElement extends SettingsAiPageElementBase {
  static get properties() {
    return {
      // ... other properties ...

      mcpServerStatus_: {
        type: Object,
        value: () => ({
          running: false,
          port: 9224,
        }),
      },
    };
  }

  declare private mcpServerStatus_: {running: boolean, port: number};

  private getMcpServerSubLabel_(running: boolean, port: number): string {
    if (running) {
      return loadTimeData.getStringF('mcpServerRunningOnPort', port);
    }
    return loadTimeData.getString('mcpServerStopped');
  }
}
```

**Key Properties:**
- `mcpServerStatus_`: Polymer property holding server state
  - `running`: Boolean indicating if server is active
  - `port`: Port number (default: 9224)

**Methods:**
- `getMcpServerSubLabel_()`: Computes sublabel text based on server state

#### 3.3 Localized Strings

**File:** `chrome/app/settings_strings.grdp:4529-4544`

```xml
<!-- MCP Server Control -->
<message name="IDS_SETTINGS_MCP_SERVER_SECTION_TITLE"
         desc="Title for the MCP Server control section in AI innovations settings">
  MCP Server (Model Control Protocol)
</message>

<message name="IDS_SETTINGS_MCP_SERVER_LABEL"
         desc="Label for the MCP Server toggle">
  Enable MCP Server
</message>

<message name="IDS_SETTINGS_MCP_SERVER_STOPPED"
         desc="Sublabel when MCP Server is stopped">
  Stopped
</message>

<message name="IDS_SETTINGS_MCP_SERVER_RUNNING_ON_PORT"
         desc="Sublabel when MCP Server is running. $1 is the port number.">
  Running on port <ph name="PORT">$1<ex>9224</ex></ph>
</message>

<message name="IDS_SETTINGS_MCP_SERVER_CONNECTION_INFO"
         desc="Label for connection information display">
  Connection information:
</message>
```

**String Registration:**

**File:** `chrome/browser/ui/webui/settings/settings_localized_strings_provider.cc:483-488`

```cpp
void AddAiStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    // ... other strings ...

    // MCP Server strings
    {"mcpServerSectionTitle", IDS_SETTINGS_MCP_SERVER_SECTION_TITLE},
    {"mcpServerLabel", IDS_SETTINGS_MCP_SERVER_LABEL},
    {"mcpServerStopped", IDS_SETTINGS_MCP_SERVER_STOPPED},
    {"mcpServerRunningOnPort", IDS_SETTINGS_MCP_SERVER_RUNNING_ON_PORT},
    {"mcpServerConnectionInfo", IDS_SETTINGS_MCP_SERVER_CONNECTION_INFO},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}
```

---

## Unit Tests

**File:** `chrome/browser/mcp_server/mcp_server_unittest.cc`

**Test Coverage (20 tests):**

```cpp
// Basic functionality tests
TEST_F(MCPServerTest, InstanceExists)
TEST_F(MCPServerTest, DefaultState)
TEST_F(MCPServerTest, StartServer)
TEST_F(MCPServerTest, StopServer)
TEST_F(MCPServerTest, StartServerCustomPort)
TEST_F(MCPServerTest, GetPortWhenRunning)
TEST_F(MCPServerTest, GetPortWhenNotRunning)

// Preference integration tests
TEST_F(MCPServerTest, SetPrefService)
TEST_F(MCPServerTest, IsEnabledInPrefsWhenNoPrefService)
TEST_F(MCPServerTest, IsEnabledInPrefsWhenDisabled)
TEST_F(MCPServerTest, IsEnabledInPrefsWhenEnabled)
TEST_F(MCPServerTest, SetEnabledInPrefs)
TEST_F(MCPServerTest, StartServerSavesStateToPrefs)
TEST_F(MCPServerTest, StopServerSavesStateToPrefs)

// Port validation tests
TEST_F(MCPServerTest, InvalidPortTooLow)
TEST_F(MCPServerTest, InvalidPortTooHigh)
TEST_F(MCPServerTest, ValidPortMinimum)
TEST_F(MCPServerTest, ValidPortMaximum)

// Multiple lifecycle tests
TEST_F(MCPServerTest, MultipleStartCalls)
TEST_F(MCPServerTest, MultipleStopCalls)
```

**Test Setup:**
```cpp
class MCPServerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    server_ = mcp_server::MCPServer::GetInstance();
    server_->SetPrefService(&pref_service_);
  }

  MCPServer* server_;
  TestingPrefServiceSimple pref_service_;
};
```

**Running Tests:**
```bash
autoninja -C out/Default chrome/browser/mcp_server:unit_tests
out/Default/chrome/browser/mcp_server:unit_tests
```

---

## Build Configuration

**File:** `chrome/browser/mcp_server/BUILD.gn`

```gn
# MCP Server - AI Agent Control Server
source_set("mcp_server") {
  sources = [
    "mcp_server.cc",
    "mcp_server.h",
  ]

  deps = [
    "//base",
    "//chrome/common:constants",
    "//components/prefs",
    "//content/public/browser",
  ]
}

# Unit tests
source_set("unit_tests") {
  testonly = true

  sources = [
    "mcp_server_unittest.cc",
  ]

  deps = [
    ":mcp_server",
    "//base",
    "//base/test:test_support",
    "//chrome/common:constants",
    "//components/prefs:test_support",
    "//content/test:test_support",
    "//testing/gtest",
  ]
}
```

**Integration:**

**File:** `chrome/browser/BUILD.gn:1869`
```gn
deps = [
  # ... other deps ...
  "//chrome/browser/mcp_server:mcp_server",
]
```

**File:** `chrome/browser/BUILD.gn:1577`
```gn
allow_circular_includes_from = [
  # ... other includes ...
  "//chrome/browser/mcp_server:mcp_server",
]
```

---

## Implementation Status

### ✅ Completed (Week 1)

1. **Project Structure** - Complete file hierarchy
2. **Feature Flag** - `chrome://flags#mcp-server` (in `about_flags.cc`)
3. **Settings UI** - Integrated into AI page at `chrome://settings/ai`
4. **Preferences Storage** - Full persistence with unit tests
5. **Core Server Class** - Lifecycle management and state tracking

### 🔄 In Progress

- **HTTP Server** (Week 2) - Localhost server on port 9224
- **WebSocket Server** (Week 2) - Real-time communication at ws://127.0.0.1:9224/ws

### 📋 TODO

**Week 2: Tab Management**
- API Dispatcher/Router
- Tab listing endpoints
- Tab creation/closing
- Tab navigation

**Week 3: UI Interactions**
- Action Runner (click, type, scroll)
- DOM Query (element inspection)
- Screenshot capture

**Week 4: Monitoring**
- Log Collector (console logs)
- Network Tracer (request tracking)

**Week 5: Testing & Documentation**
- Integration tests
- API documentation
- Usage examples

**Week 6: Polish & Release**
- Performance optimization
- Security review
- Release preparation

---

## API Endpoints (Planned)

### HTTP Endpoints

```
GET  /mcp/health              - Health check
GET  /mcp/tabs                - List all tabs
POST /mcp/tabs                - Create new tab
GET  /mcp/tabs/:id            - Get tab details
PUT  /mcp/tabs/:id/navigate   - Navigate tab to URL
POST /mcp/tabs/:id/click      - Click element
POST /mcp/tabs/:id/type       - Type text
GET  /mcp/tabs/:id/screenshot - Capture screenshot
GET  /mcp/tabs/:id/dom        - Get DOM tree
DELETE /mcp/tabs/:id          - Close tab
```

### WebSocket Events

```
connect     - Client connected
tab.created - New tab created
tab.updated - Tab state changed
tab.closed  - Tab closed
console.log - Console message
network.request  - Network request started
network.response - Network response received
```

---

## Development Workflow

### Building Chrome

```bash
# Full build
autoninja -C out/Default chrome

# Incremental build (after changes)
autoninja -C out/Default chrome

# Build and run tests
autoninja -C out/Default chrome/browser/mcp_server:unit_tests
out/Default/chrome/browser/mcp_server:unit_tests
```

### Testing the UI

1. Build Chrome: `autoninja -C out/Default chrome`
2. Run: `./out/Default/Chromium.app/Contents/MacOS/Chromium`
3. Navigate to: `chrome://settings/ai`
4. Scroll to "MCP Server (Model Control Protocol)" section
5. Toggle "Enable MCP Server"
6. Verify connection info appears when enabled

### Code Style

- Follow [Chromium C++ Style Guide](https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++.md)
- Use `clang-format` for formatting
- Run `git cl format` before committing

---

## Security Considerations

1. **Localhost Only** - Server binds only to 127.0.0.1
2. **No Remote Access** - No external network exposure
3. **Preference-Gated** - Disabled by default, requires user opt-in
4. **Port Range Validation** - Only unprivileged ports (1024-65535)
5. **Future: Authentication** - Plan to add token-based auth in Week 5

---

## Troubleshooting

### Build Errors

**Error:** `unknown target chrome/browser/mcp_server`
**Fix:** Add to `chrome/browser/BUILD.gn` deps

**Error:** `Circular dependency`
**Fix:** Add to `allow_circular_includes_from` in `chrome/browser/BUILD.gn`

**Error:** `Undefined identifier enable_mcp_server`
**Fix:** Remove conditional compilation from `BUILD.gn`

### Runtime Issues

**Issue:** Settings UI doesn't appear
**Check:**
1. Verify strings are registered in `settings_localized_strings_provider.cc`
2. Check `ai_page.html` template syntax
3. Ensure TypeScript properties are declared correctly

**Issue:** Preferences not saving
**Check:**
1. Verify `SetPrefService()` was called
2. Check preference keys match in `pref_names.h` and usage
3. Ensure preferences are registered in `browser_prefs.cc`

---

## Resources

- **Design Doc:** `chrome/browser/mcp_server/README.md`
- **Implementation:** This file
- **Tests:** `chrome/browser/mcp_server/mcp_server_unittest.cc`
- **API Spec:** (TODO: Week 5)

---

## Contact

For questions or contributions, please refer to the main README.md or file a bug in the Chromium issue tracker.

**Last Updated:** January 11, 2026
**Status:** Week 1 Complete, Week 2 In Progress

#!/bin/bash
# MCP Server Integration Test Suite
# Tests all Dispatcher and Tab Controller endpoints

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Configuration
MCP_SERVER_URL="http://localhost:9224"
CHROME_USER_DATA_DIR="/tmp/chrome-mcp-test-$$"
# Get the script directory and find Chrome binary
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
CHROME_BINARY="$SRC_DIR/out/Default/Chromium.app/Contents/MacOS/Chromium"
CHROME_PID=""

# Helper functions
print_header() {
    echo -e "\n${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}\n"
}

print_test() {
    echo -e "${YELLOW}[TEST $((TOTAL_TESTS + 1))]${NC} $1"
}

print_pass() {
    echo -e "${GREEN}✓ PASS${NC} $1"
    ((PASSED_TESTS++))
    ((TOTAL_TESTS++))
}

print_fail() {
    echo -e "${RED}✗ FAIL${NC} $1"
    echo -e "${RED}  Expected: $2${NC}"
    echo -e "${RED}  Got: $3${NC}"
    ((FAILED_TESTS++))
    ((TOTAL_TESTS++))
}

print_info() {
    echo -e "${BLUE}ℹ${NC} $1"
}

print_error() {
    echo -e "${RED}ERROR:${NC} $1"
}

# Cleanup function
cleanup() {
    print_info "Cleaning up..."
    if [ ! -z "$CHROME_PID" ] && kill -0 $CHROME_PID 2>/dev/null; then
        print_info "Stopping Chrome (PID: $CHROME_PID)..."
        kill -9 $CHROME_PID 2>/dev/null || true
        wait $CHROME_PID 2>/dev/null || true
    fi

    # Clean up Chrome user data directory
    if [ -d "$CHROME_USER_DATA_DIR" ]; then
        rm -rf "$CHROME_USER_DATA_DIR"
    fi

    print_info "Cleanup complete"
}

# Set trap to cleanup on exit
trap cleanup EXIT INT TERM

# Start Chrome with MCP Server enabled
start_chrome() {
    print_header "Starting Chrome with MCP Server"

    # Kill any existing Chrome instances
    pkill -9 Chromium 2>/dev/null || true
    sleep 1

    # Create user data directory
    mkdir -p "$CHROME_USER_DATA_DIR"

    # Enable MCP Server in preferences
    print_info "Enabling MCP Server in preferences..."
    python3 <<EOF
import json
import os

local_state_path = '$CHROME_USER_DATA_DIR/Local State'
data = {
    'ai_features': {
        'mcp_server_enabled': True,
        'mcp_server_port': 9224
    }
}

os.makedirs(os.path.dirname(local_state_path), exist_ok=True)
with open(local_state_path, 'w') as f:
    json.dump(data, f, indent=2)

print("✓ MCP Server enabled in Local State")
EOF

    # Start Chrome
    print_info "Starting Chrome..."
    "$CHROME_BINARY" \
        --user-data-dir="$CHROME_USER_DATA_DIR" \
        --disable-gpu \
        --no-first-run \
        --no-default-browser-check \
        > /dev/null 2>&1 &

    CHROME_PID=$!
    print_info "Chrome started (PID: $CHROME_PID)"

    # Wait for Chrome and MCP Server to start
    print_info "Waiting for MCP Server to be ready..."
    local max_attempts=30
    local attempt=0

    while [ $attempt -lt $max_attempts ]; do
        if curl -s "$MCP_SERVER_URL/" > /dev/null 2>&1; then
            print_info "MCP Server is ready!"
            return 0
        fi
        sleep 1
        ((attempt++))
        echo -n "."
    done

    echo ""
    print_error "MCP Server failed to start within 30 seconds"
    return 1
}

# Test helper function
run_test() {
    local test_name="$1"
    local method="$2"
    local path="$3"
    local data="$4"
    local expected_field="$5"
    local expected_value="$6"

    print_test "$test_name"

    # Make HTTP request
    local response
    if [ "$method" = "GET" ]; then
        response=$(curl -s "$MCP_SERVER_URL$path")
    elif [ "$method" = "POST" ]; then
        response=$(curl -s -X POST "$MCP_SERVER_URL$path" \
            -H "Content-Type: application/json" \
            -d "$data")
    elif [ "$method" = "DELETE" ]; then
        response=$(curl -s -X DELETE "$MCP_SERVER_URL$path")
    fi

    # Validate response
    if [ ! -z "$expected_field" ]; then
        local actual_value=$(echo "$response" | python3 -c "import sys, json; data = json.load(sys.stdin); print(data.get('$expected_field', ''))" 2>/dev/null || echo "")

        if [ "$actual_value" = "$expected_value" ]; then
            print_pass "$test_name"
        else
            print_fail "$test_name" "$expected_value" "$actual_value"
        fi
    else
        # Just check if response is valid JSON
        if echo "$response" | python3 -m json.tool > /dev/null 2>&1; then
            print_pass "$test_name"
        else
            print_fail "$test_name" "Valid JSON" "Invalid JSON: $response"
        fi
    fi

    # Store response for next tests
    echo "$response"
}

# JSON validation helper
validate_json_field() {
    local response="$1"
    local field="$2"
    local expected="$3"
    local test_name="$4"

    local actual=$(echo "$response" | python3 -c "import sys, json; data = json.load(sys.stdin); print(data.get('$field', ''))" 2>/dev/null || echo "")

    if [ "$actual" = "$expected" ]; then
        print_pass "$test_name"
        return 0
    else
        print_fail "$test_name" "$expected" "$actual"
        return 1
    fi
}

# Main test suite
run_tests() {
    print_header "MCP Server Integration Tests"

    # Test 1: Server Info
    local response
    response=$(run_test "GET / - Server Info" "GET" "/" "" "name" "MCP Server")

    # Test 2: Health Check
    response=$(run_test "GET /health - Health Check" "GET" "/health" "" "status" "ok")

    # Test 3: List Tabs (initially should have 1 new tab)
    print_test "GET /mcp/tabs - List Tabs (initial)"
    response=$(curl -s "$MCP_SERVER_URL/mcp/tabs")
    local tab_count=$(echo "$response" | python3 -c "import sys, json; data = json.load(sys.stdin); print(len(data.get('tabs', [])))" 2>/dev/null || echo "0")

    if [ "$tab_count" -ge "1" ]; then
        print_pass "Initial tabs present (count: $tab_count)"
    else
        print_fail "Initial tabs present" "At least 1 tab" "$tab_count tabs"
    fi

    # Test 4: Create Tab
    print_test "POST /mcp/tabs - Create Tab"
    response=$(curl -s -X POST "$MCP_SERVER_URL/mcp/tabs" \
        -H "Content-Type: application/json" \
        -d '{"url": "https://example.com"}')

    local new_tab_id=$(echo "$response" | python3 -c "import sys, json; data = json.load(sys.stdin); print(data.get('id', ''))" 2>/dev/null || echo "")
    local new_tab_url=$(echo "$response" | python3 -c "import sys, json; data = json.load(sys.stdin); print(data.get('url', ''))" 2>/dev/null || echo "")

    if [ ! -z "$new_tab_id" ] && [[ "$new_tab_url" == *"example.com"* ]]; then
        print_pass "Tab created successfully (ID: $new_tab_id)"
    else
        print_fail "Tab created" "Valid tab with example.com URL" "ID: $new_tab_id, URL: $new_tab_url"
    fi

    # Wait for page to load
    sleep 2

    # Test 5: List Tabs (should now have 2+ tabs)
    print_test "GET /mcp/tabs - List Tabs (after create)"
    response=$(curl -s "$MCP_SERVER_URL/mcp/tabs")
    local new_tab_count=$(echo "$response" | python3 -c "import sys, json; data = json.load(sys.stdin); print(len(data.get('tabs', [])))" 2>/dev/null || echo "0")

    if [ "$new_tab_count" -ge "2" ]; then
        print_pass "Tab list updated (count: $new_tab_count)"
    else
        print_fail "Tab list updated" "At least 2 tabs" "$new_tab_count tabs"
    fi

    # Test 6: Get Tab State
    if [ ! -z "$new_tab_id" ]; then
        print_test "GET /mcp/tabs/:id/state - Get Tab State"
        response=$(curl -s "$MCP_SERVER_URL/mcp/tabs/$new_tab_id/state")
        local tab_title=$(echo "$response" | python3 -c "import sys, json; data = json.load(sys.stdin); print(data.get('title', ''))" 2>/dev/null || echo "")

        if [[ "$tab_title" == *"Example"* ]] || [ ! -z "$tab_title" ]; then
            print_pass "Tab state retrieved (title: $tab_title)"
        else
            print_fail "Tab state retrieved" "Non-empty title" "Empty or missing"
        fi
    fi

    # Test 7: Get first tab ID for activation test
    print_test "Finding first tab for activation test"
    response=$(curl -s "$MCP_SERVER_URL/mcp/tabs")
    local first_tab_id=$(echo "$response" | python3 -c "import sys, json; data = json.load(sys.stdin); tabs = data.get('tabs', []); print(tabs[0]['id'] if tabs else '')" 2>/dev/null || echo "")

    if [ ! -z "$first_tab_id" ]; then
        print_pass "First tab ID found: $first_tab_id"
    else
        print_fail "First tab ID found" "Valid tab ID" "None"
    fi

    # Test 8: Activate Tab
    if [ ! -z "$first_tab_id" ]; then
        print_test "POST /mcp/tabs/:id/activate - Activate Tab"
        response=$(curl -s -X POST "$MCP_SERVER_URL/mcp/tabs/$first_tab_id/activate")
        local success=$(echo "$response" | python3 -c "import sys, json; data = json.load(sys.stdin); print(data.get('success', False))" 2>/dev/null || echo "False")

        if [ "$success" = "True" ]; then
            print_pass "Tab activated successfully"
        else
            print_fail "Tab activated" "success: true" "success: $success"
        fi

        # Verify activation
        sleep 1
        print_test "Verify tab activation"
        response=$(curl -s "$MCP_SERVER_URL/mcp/tabs")
        local active_tab_id=$(echo "$response" | python3 -c "import sys, json; data = json.load(sys.stdin); tabs = data.get('tabs', []); active = [t for t in tabs if t.get('active')]; print(active[0]['id'] if active else '')" 2>/dev/null || echo "")

        if [ "$active_tab_id" = "$first_tab_id" ]; then
            print_pass "Tab activation verified"
        else
            print_fail "Tab activation verified" "Active tab ID: $first_tab_id" "Active tab ID: $active_tab_id"
        fi
    fi

    # Test 9: Close Tab
    if [ ! -z "$new_tab_id" ]; then
        print_test "DELETE /mcp/tabs/:id - Close Tab"
        response=$(curl -s -X DELETE "$MCP_SERVER_URL/mcp/tabs/$new_tab_id")
        local success=$(echo "$response" | python3 -c "import sys, json; data = json.load(sys.stdin); print(data.get('success', False))" 2>/dev/null || echo "False")

        if [ "$success" = "True" ]; then
            print_pass "Tab closed successfully"
        else
            print_fail "Tab closed" "success: true" "success: $success"
        fi

        # Verify tab is gone
        sleep 1
        print_test "Verify tab closure"
        response=$(curl -s "$MCP_SERVER_URL/mcp/tabs/$new_tab_id/state")
        local error=$(echo "$response" | python3 -c "import sys, json; data = json.load(sys.stdin); print(data.get('error', ''))" 2>/dev/null || echo "")

        if [[ "$error" == *"not found"* ]]; then
            print_pass "Tab closure verified"
        else
            print_fail "Tab closure verified" "Tab not found error" "Response: $error"
        fi
    fi

    # Test 10: Error - Invalid Tab ID
    print_test "GET /mcp/tabs/99999/state - Invalid Tab ID"
    response=$(curl -s "$MCP_SERVER_URL/mcp/tabs/99999/state")
    local error=$(echo "$response" | python3 -c "import sys, json; data = json.load(sys.stdin); print('error' in data)" 2>/dev/null || echo "False")

    if [ "$error" = "True" ]; then
        print_pass "Invalid tab ID returns error"
    else
        print_fail "Invalid tab ID returns error" "Error response" "No error in response"
    fi

    # Test 11: Error - Invalid Route
    print_test "GET /invalid/path - Invalid Route"
    response=$(curl -s "$MCP_SERVER_URL/invalid/path")
    local error=$(echo "$response" | python3 -c "import sys, json; data = json.load(sys.stdin); print(data.get('error', ''))" 2>/dev/null || echo "")

    if [[ "$error" == *"not found"* ]] || [[ "$error" == *"Route not found"* ]]; then
        print_pass "Invalid route returns 404"
    else
        print_fail "Invalid route returns 404" "Route not found" "$error"
    fi

    # Test 12: Error - Invalid JSON
    print_test "POST /mcp/tabs - Invalid JSON"
    response=$(curl -s -X POST "$MCP_SERVER_URL/mcp/tabs" \
        -H "Content-Type: application/json" \
        -d '{invalid json}')
    local error=$(echo "$response" | python3 -c "import sys, json; data = json.load(sys.stdin); print(data.get('error', ''))" 2>/dev/null || echo "")

    if [[ "$error" == *"Invalid JSON"* ]]; then
        print_pass "Invalid JSON returns error"
    else
        print_fail "Invalid JSON returns error" "Invalid JSON error" "$error"
    fi

    # Test 13: Error - Missing Required Field
    print_test "POST /mcp/tabs - Missing URL Field"
    response=$(curl -s -X POST "$MCP_SERVER_URL/mcp/tabs" \
        -H "Content-Type: application/json" \
        -d '{}')
    local error=$(echo "$response" | python3 -c "import sys, json; data = json.load(sys.stdin); print(data.get('error', ''))" 2>/dev/null || echo "")

    if [[ "$error" == *"url"* ]] || [[ "$error" == *"Missing"* ]]; then
        print_pass "Missing required field returns error"
    else
        print_fail "Missing required field returns error" "Missing field error" "$error"
    fi

    # Test 14: Create Multiple Tabs
    print_test "POST /mcp/tabs - Create Multiple Tabs"
    local urls=("https://www.google.com" "https://github.com" "https://stackoverflow.com")
    local created_count=0

    for url in "${urls[@]}"; do
        response=$(curl -s -X POST "$MCP_SERVER_URL/mcp/tabs" \
            -H "Content-Type: application/json" \
            -d "{\"url\": \"$url\"}")
        local tab_id=$(echo "$response" | python3 -c "import sys, json; data = json.load(sys.stdin); print(data.get('id', ''))" 2>/dev/null || echo "")

        if [ ! -z "$tab_id" ]; then
            ((created_count++))
        fi
    done

    if [ "$created_count" -eq "3" ]; then
        print_pass "Created 3 tabs successfully"
    else
        print_fail "Created 3 tabs" "3 tabs" "$created_count tabs"
    fi
}

# Print test summary
print_summary() {
    print_header "Test Summary"

    echo -e "Total Tests:  ${BLUE}$TOTAL_TESTS${NC}"
    echo -e "Passed:       ${GREEN}$PASSED_TESTS${NC}"
    echo -e "Failed:       ${RED}$FAILED_TESTS${NC}"

    if [ $FAILED_TESTS -eq 0 ]; then
        echo -e "\n${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
        echo -e "${GREEN}✓ ALL TESTS PASSED!${NC}"
        echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}\n"
        return 0
    else
        echo -e "\n${RED}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
        echo -e "${RED}✗ SOME TESTS FAILED${NC}"
        echo -e "${RED}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}\n"
        return 1
    fi
}

# Main execution
main() {
    # Check if Chrome binary exists
    if [ ! -f "$CHROME_BINARY" ]; then
        print_error "Chrome binary not found at: $CHROME_BINARY"
        print_info "Please build Chrome first: autoninja -C out/Default chrome"
        exit 1
    fi

    # Check if python3 is available
    if ! command -v python3 &> /dev/null; then
        print_error "python3 is required but not installed"
        exit 1
    fi

    # Start Chrome
    if ! start_chrome; then
        print_error "Failed to start Chrome with MCP Server"
        exit 1
    fi

    # Run tests
    run_tests

    # Print summary
    print_summary

    # Return exit code based on test results
    if [ $FAILED_TESTS -eq 0 ]; then
        exit 0
    else
        exit 1
    fi
}

# Run main function
main "$@"

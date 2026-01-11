#!/bin/bash
# Quick MCP Server Test
# Assumes Chrome with MCP Server is already running on localhost:9224

set -e

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

MCP_SERVER_URL="http://localhost:9224"

echo -e "${BLUE}Quick MCP Server Test${NC}\n"

# Check if server is running
if ! curl -s "$MCP_SERVER_URL/" > /dev/null 2>&1; then
    echo -e "${RED}✗ MCP Server is not running on $MCP_SERVER_URL${NC}"
    echo -e "Please start Chrome with MCP Server enabled first."
    exit 1
fi

echo -e "${GREEN}✓ MCP Server is running${NC}\n"

# Test 1: Server info
echo "Test 1: GET /"
curl -s "$MCP_SERVER_URL/" | python3 -m json.tool
echo ""

# Test 2: Health check
echo "Test 2: GET /health"
curl -s "$MCP_SERVER_URL/health" | python3 -m json.tool
echo ""

# Test 3: List tabs
echo "Test 3: GET /mcp/tabs"
tabs_response=$(curl -s "$MCP_SERVER_URL/mcp/tabs")
echo "$tabs_response" | python3 -m json.tool
echo ""

# Test 4: Create tab
echo "Test 4: POST /mcp/tabs (Create tab)"
create_response=$(curl -s -X POST "$MCP_SERVER_URL/mcp/tabs" \
    -H "Content-Type: application/json" \
    -d '{"url": "https://example.com"}')
echo "$create_response" | python3 -m json.tool

# Extract tab ID for subsequent tests
tab_id=$(echo "$create_response" | python3 -c "import sys, json; print(json.load(sys.stdin).get('id', ''))" 2>/dev/null)
echo -e "\nCreated tab ID: $tab_id\n"

# Wait for page to load
sleep 2

# Test 5: Get tab state
if [ ! -z "$tab_id" ]; then
    echo "Test 5: GET /mcp/tabs/$tab_id/state"
    curl -s "$MCP_SERVER_URL/mcp/tabs/$tab_id/state" | python3 -m json.tool
    echo ""
fi

# Test 6: List tabs again
echo "Test 6: GET /mcp/tabs (after creating tab)"
curl -s "$MCP_SERVER_URL/mcp/tabs" | python3 -m json.tool
echo ""

# Test 7: Activate tab
if [ ! -z "$tab_id" ]; then
    echo "Test 7: POST /mcp/tabs/$tab_id/activate"
    curl -s -X POST "$MCP_SERVER_URL/mcp/tabs/$tab_id/activate" | python3 -m json.tool
    echo ""
fi

# Test 8: Close tab
if [ ! -z "$tab_id" ]; then
    echo "Test 8: DELETE /mcp/tabs/$tab_id (Close tab)"
    curl -s -X DELETE "$MCP_SERVER_URL/mcp/tabs/$tab_id" | python3 -m json.tool
    echo ""
fi

# Test 9: Error case - invalid tab ID
echo "Test 9: GET /mcp/tabs/99999/state (Error case)"
curl -s "$MCP_SERVER_URL/mcp/tabs/99999/state" | python3 -m json.tool
echo ""

# Test 10: Error case - invalid route
echo "Test 10: GET /invalid/path (Error case)"
curl -s "$MCP_SERVER_URL/invalid/path" | python3 -m json.tool
echo ""

# Test 11: Error case - missing field
echo "Test 11: POST /mcp/tabs with missing URL (Error case)"
curl -s -X POST "$MCP_SERVER_URL/mcp/tabs" \
    -H "Content-Type: application/json" \
    -d '{}' | python3 -m json.tool
echo ""

echo -e "${GREEN}✓ All quick tests completed!${NC}"

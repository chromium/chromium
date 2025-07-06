# server.py
import socket
import os
import json

SOCKET_PATH = "/sockets/echo_socket"

# Remove existing socket if present
if os.path.exists(SOCKET_PATH):
    os.remove(SOCKET_PATH)

# Create UNIX domain socket
server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
server.bind(SOCKET_PATH)
server.listen(1)

print(f"Server listening on {SOCKET_PATH}")

try:
    while True:
        conn, _ = server.accept()
        with conn:
            print("Client connected")

            data = conn.recv(1024)
            if not data:
                print("No data received")
                continue

            message = data.decode().strip()
            print(f"Received: {message}")

            # if data.decode().strip() == "GET /data":
            response_data = {"status": True, "key": "value", "message": message.upper()}
            response_json = json.dumps(response_data)

            conn.sendall(response_json.encode())
            print("Sent JSON response")
            # else:
                # conn.sendall(b"Unknown command")
except KeyboardInterrupt:
    print("\nShutting down server")
finally:
    server.close()
    os.remove(SOCKET_PATH)
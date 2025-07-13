import os
import socket
import json
from types_defs import Payload
from router import route
from response import respond

SOCKET_PATH = "/sockets/echo_socket"

def run_socket_server():
    if os.path.exists(SOCKET_PATH):
        os.remove(SOCKET_PATH)

    server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    server.bind(SOCKET_PATH)
    server.listen(1)

    print(f"Listening on {SOCKET_PATH}")

    try:
        while True:
            conn, _ = server.accept()
            with conn:
                data = conn.recv(1024)
                if not data:
                    respond(conn, "error", "No data received")
                    continue

                try:
                    payload: Payload = json.loads(data.decode())
                    route(conn, payload)
                except json.JSONDecodeError:
                    respond(conn, "error", "Malformed JSON")
                except Exception as e:
                    respond(conn, "error", f"Unhandled server error: {e}")
    finally:
        server.close()
        os.remove(SOCKET_PATH)

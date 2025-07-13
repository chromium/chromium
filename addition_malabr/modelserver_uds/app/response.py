import socket
import json
from typing import Literal

def respond(conn: socket.socket, status: Literal["ok", "error"], message: str) -> None:
    data = json.dumps({"status": status, "message": message})
    conn.sendall(data.encode('utf-8'))
